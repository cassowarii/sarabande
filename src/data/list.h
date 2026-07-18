#include "common.h"

void sbList_sys_init();

void sbList_sys_deinit();

hList sbList_new(usize capacity);

hList sbList_of(usize length, hVal *items);

void sbList_append(hList list, hVal *item);

hVal *sbList_get_value(hList list, usize *length);

hVal *sbList_index(hList list, usize index);

void sbList_method(hVm vm);
