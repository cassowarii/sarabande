#include "data/closure.h"

#include "data/reference.h"
#include "gc/gcinfo.h"

#define FLAG_REAL (1ULL << 63)

#define CLOSURES_PER_BLOCK 256
#define INLINE_VAR_COUNT 16

/* TODO eliminate globals */
sbPool g_closure_pool = {0};

typedef struct sbClosure {
  GCINFO info;
  usize num_vars;
  union {
    hRef internal_vars[INLINE_VAR_COUNT];
    hRef *external_vars;
  };
} sbClosure;

static sbClosure *find_closure_by_handle(hRef handle);

void sbClosure_sys_init() {
  sbPool_initialize(&g_closure_pool, sizeof(sbClosure), CLOSURES_PER_BLOCK);
}

void sbClosure_sys_deinit() {
  //sbPool_deinitialize(&g_closure_pool);
}

hClosure sbClosure_create(usize num_vars) {
  usize index;
  sbClosure *c = sbPool_alloc(&g_closure_pool, &index);
  c->num_vars = num_vars;
  if (num_vars > INLINE_VAR_COUNT) {
    c->external_vars = calloc(num_vars, sizeof(hRef));
  }
  return index | FLAG_REAL;
}

/* set the variable in the closure behind the pointer */
void sbClosure_set_var(hClosure which, usize index, hVal *what) {
  sbClosure *c = find_closure_by_handle(which);
  if (c->num_vars <= INLINE_VAR_COUNT) {
    sbRef_set_ref(c->internal_vars[index], what);
  } else {
    sbRef_set_ref(c->external_vars[index], what);
  }
}

/* set the pointer itself */
void sbClosure_set_ref(hClosure which, usize index, hVal *what) {
  if (what->type != IT_REF) {
    PANIC("must pass an indirect variable to be the subject of a closure");
  }
  sbClosure *c = find_closure_by_handle(which);
  if (c->num_vars <= INLINE_VAR_COUNT) {
    c->internal_vars[index] = what->ref;
  } else {
    c->external_vars[index] = what->ref;
  }
}

hVal *sbClosure_get_var(hClosure which, usize index) {
  sbClosure *c = find_closure_by_handle(which);
  if (c->num_vars <= INLINE_VAR_COUNT) {
    return sbRef_deref(c->internal_vars[index]);
  } else {
    return sbRef_deref(c->external_vars[index]);
  }
}

hVal sbClosure_get_ref(hClosure which, usize index) {
  sbClosure *c = find_closure_by_handle(which);
  if (c->num_vars <= INLINE_VAR_COUNT) {
    return HVREF(c->internal_vars[index]);
  } else {
    return HVREF(c->external_vars[index]);
  }
}

/* --- */

static sbClosure *find_closure_by_handle(hRef handle) {
  if (!(handle & FLAG_REAL)) PANIC("null closure");
  return sbPool_get_entry(&g_closure_pool, handle & ~FLAG_REAL);
}
