#include "data/list.h"

#include "gc/gcinfo.h"
#include "vm/exec.h"
#include "mem/debug.h"

#define LIST_PER_BLOCK 256
#define METHOD_IS(name) (!sbstrncmp(method_name, name, sizeof(name)))

/* TODO etc etc */
sbPool g_list_pool;

typedef struct sbList {
  GCINFO gc;
  u64 handle;
  sbBuffer items;
} sbList;

sbList *get_list_by_handle(hList handle);

void sbList_sys_init() {
  sbPool_initialize(&g_list_pool, sizeof(sbList), LIST_PER_BLOCK);
}

void sbList_sys_deinit() {
  //sbPool_deinitialize(&g_list_pool);
}

hList sbList_new(usize capacity) {
  usize index;
  sbList *l = sbPool_alloc(&g_list_pool, &index);
  if (capacity < 4) capacity = 4;
  sbBuffer_initialize(&l->items, capacity * sizeof(sbVar));
  return index;
}

hList sbList_of(usize length, hVar items) {
  usize index;
  usize capacity = length;
  sbList *l = sbPool_alloc(&g_list_pool, &index);
  if (capacity < 4) capacity = 4;
  sbBuffer_initialize(&l->items, capacity * sizeof(sbVar));
  sbBuffer_set_size(&l->items, length * sizeof(sbVar));
  memcpy(l->items.data, items, length * sizeof(sbVar));
  return index;
}

void sbList_append(hList list, hVal *item) {
  sbList *l = get_list_by_handle(list);
  sbV_retain(item);
  sbVar new_var = { .value = *item };
  sbBuffer_append(&l->items, &new_var, sizeof(sbVar));
}

sbVar *sbList_get_value(hList list, usize *length) {
  sbList *l = get_list_by_handle(list);
  if (length) *length = l->items.size / sizeof(sbVar);
  return (sbVar*)l->items.data;
}

hVal sbList_index_value(hList list, usize index) {
  sbList *l = get_list_by_handle(list);
  hVar item = &((sbVar*)l->items.data)[index];
  return sbVar_get_value(item);
}

hVal sbList_index_lvalue_ref(hList list, usize index) {
  sbList *l = get_list_by_handle(list);
  hVar item = &((sbVar*)l->items.data)[index];
  return sbVar_get_lvalue_ref(item);
}

hVal sbList_index_rvalue_ref(hList list, usize index) {
  sbList *l = get_list_by_handle(list);
  hVar item = &((sbVar*)l->items.data)[index];
  return sbVar_get_rvalue_ref(item);
}

/* --- */

sbList *get_list_by_handle(hList handle) {
  return sbPool_get_entry(&g_list_pool, handle);
}
