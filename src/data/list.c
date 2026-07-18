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
  sbBuffer_initialize(&l->items, capacity * sizeof(hVal));
  return index;
}

hList sbList_of(usize length, hVal *items) {
  usize index;
  usize capacity = length;
  sbList *l = sbPool_alloc(&g_list_pool, &index);
  if (capacity < 4) capacity = 4;
  sbBuffer_initialize(&l->items, capacity * sizeof(hVal));
  sbBuffer_set_size(&l->items, length * sizeof(hVal));
  memcpy(l->items.data, items, length * sizeof(hVal));
  return index;
}

void sbList_append(hList list, hVal *item) {
  sbList *l = get_list_by_handle(list);
  sbV_retain(item);
  sbBuffer_append(&l->items, item, sizeof(hVal));
}

hVal *sbList_get_value(hList list, usize *length) {
  sbList *l = get_list_by_handle(list);
  if (length) *length = l->items.size / sizeof(hVal);
  return (hVal*)l->items.data;
}

hVal *sbList_index(hList list, usize index) {
  sbList *l = get_list_by_handle(list);
  hVal *item = &((hVal*)l->items.data)[index];
  return item;
}

/* --- */

sbList *get_list_by_handle(hList handle) {
  return sbPool_get_entry(&g_list_pool, handle);
}
