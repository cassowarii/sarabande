#include "data/list.h"

#include "gc/gcinfo.h"

#define LIST_PER_BLOCK 256

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
  sbBuffer_initialize(&l->items, capacity * sizeof(hV));
  return index;
}

void sbList_append(hList list, hV *item) {
  sbList *l = get_list_by_handle(list);
  sbV_retain(item);
  sbBuffer_append(&l->items, item, sizeof(hV));
}

hV *sbList_get_value(hList list, usize *length) {
  sbList *l = get_list_by_handle(list);
  if (length) *length = l->items.size / sizeof(hV);
  return (hV*)l->items.data;
}

hV sbList_index(hList list, usize index) {
  sbList *l = get_list_by_handle(list);
  hV *item = &((hV*)l->items.data)[index];
  sbV_retain(item);
  return *item;
}

/* --- */

sbList *get_list_by_handle(hList handle) {
  return sbPool_get_entry(&g_list_pool, handle);
}
