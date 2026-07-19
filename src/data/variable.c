#include "common.h"

#include "gc/gcinfo.h"

#define VARS_PER_BLOCK 1024

typedef struct HeapVar {
  GCINFO gcinfo;
  hVal value;
} HeapVar;

/* TODO argle bargle */
sbPool g_heapvar_pool;

HeapVar *new_heapvar(hVal *v);

void sbVar_sys_init() {
  sbPool_initialize(&g_heapvar_pool, sizeof(HeapVar), VARS_PER_BLOCK);
}

void sbVar_sys_deinit() {
  //sbPool_deinitialize(&g_heapvar_pool, sizeof(HeapVar), VARS_PER_BLOCK);
}

hVal sbVar_get_value(hVar v) {
  if (v->value.type == ITX_HEAPVAR) {
    return ((HeapVar*)v->value.ptrdata)->value;
  } else {
    return v->value;
  }
}

hVal *sbVar_get_value_ptr(hVar v) {
  if (v->value.type == ITX_HEAPVAR) {
    return &((HeapVar*)v->value.ptrdata)->value;
  } else {
    return &v->value;
  }
}

void sbVar_set_value(hVar v, const hVal *new_value) {
  /* release whatever was in the slot before */
  sbV_release(&v->value);
  sbV_retain(new_value);
  if (v->value.type == ITX_HEAPVAR) {
    ((HeapVar*)v->value.ptrdata)->value = *new_value;
  } else {
    v->value = *new_value;
  }
}

/* for stuff like bound methods: we attach a pointer to a heap var to store
 * what they're attached to, but don't bother allocating a full variable
 * anywhere */
void sbVar_set_attached_ref(hVal *attach_to, hVal *value) {
  HeapVar *stored = new_heapvar(value);
  attach_to->ptrdata = stored;
}

hVal *sbVar_get_attached_ref(const hVal *attached_to) {
  return &((HeapVar*)attached_to->ptrdata)->value;
}

hVal sbVar_get_lvalue_ref(hVar v) {
  return HVREF(v);
}

void sbVar_move_to_heap(hVar v) {
  HeapVar *new_entry = new_heapvar(&v->value);
  v->value.type = ITX_HEAPVAR;
  v->value.ptrdata = new_entry;
}

hVar sbVar_deref(const hVal *v) {
  if (v->type != IT_REF) {
    CHECK("Internal error: value is not a pointer to variable");
  }

  /* v is ref */
  return (hVar)v->ptrdata;
}

sbVar sbVar_retain(hVar v) {
  if (v->value.type != ITX_HEAPVAR) {
    sbVar_move_to_heap(v);
  }

  ((HeapVar*)v->value.ptrdata)->gcinfo.refcount ++;
  return *v;
}

void sbVar_release(hVar v) {
  if (v->value.type == ITX_HEAPVAR) {
    ((HeapVar*)v->value.ptrdata)->gcinfo.refcount --;
  }
}

hVal sbVar_get_rvalue_ref(hVar v) {
  if (v->value.type != ITX_HEAPVAR) {
    sbVar_move_to_heap(v);
  }

  return HVREF(v);
}

/* --- */

HeapVar *new_heapvar(hVal *v) {
  HeapVar *new_entry = sbPool_alloc(&g_heapvar_pool, NULL);
  new_entry->gcinfo.refcount = 1;
  new_entry->value = *v;
  return new_entry;
}
