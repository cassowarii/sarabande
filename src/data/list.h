#include "common.h"

void sbList_sys_init();

void sbList_sys_deinit();

hList sbList_new(usize capacity);

void sbList_append(hList list, hV *item);

hV *sbList_get_value(hList list, usize *length);

hV *sbList_index(hList list, usize index);
