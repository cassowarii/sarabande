#include "data/reference.h"

#include "gc/gcinfo.h"

#define REFERENCES_PER_BLOCK 1024

/* TODO eliminate globals */
sbPool g_reference_pool = {0};

typedef struct sbRef {
  GCINFO info;
  hVal pointed_to;
} sbRef;

static sbRef *find_ref_by_handle(hRef handle);

void sbRef_sys_init() {
  sbPool_initialize(&g_reference_pool, sizeof(sbRef), REFERENCES_PER_BLOCK);
}

void sbRef_sys_deinit() {
  //sbPool_deinitialize(&g_reference_pool);
}

hRef sbRef_create(hVal *var) {
  usize index;
  sbRef *r = sbPool_alloc(&g_reference_pool, &index);
  sbV_retain(var);
  r->pointed_to = *var;
  return index;
}

void sbRef_set_ref(hRef ref, hVal *var) {
  sbRef *value = find_ref_by_handle(ref);
  sbV_release(&value->pointed_to);
  sbV_retain(var);
  value->pointed_to = *var;
}

hVal *sbRef_deref(hRef ref) {
  sbRef *value = find_ref_by_handle(ref);
  return &value->pointed_to;
}

/* --- */

static sbRef *find_ref_by_handle(hRef handle) {
  return sbPool_get_entry(&g_reference_pool, handle);
}
