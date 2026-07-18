#include "common.h"

void sbList_sys_init();

void sbList_sys_deinit();

hList sbList_new(usize capacity);

hList sbList_of(usize length, hVar items);

void sbList_append(hList list, hVal *item);

sbVar *sbList_get_value(hList list, usize *length);

hVal sbList_index_value(hList list, usize index);

hVal sbList_index_lvalue_ref(hList list, usize index);

hVal sbList_index_rvalue_ref(hList list, usize index);

void sbList_method(hVm vm);
