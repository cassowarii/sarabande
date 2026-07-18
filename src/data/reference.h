#include "common.h"

void sbRef_sys_init();

void sbRef_sys_deinit();

hRef sbRef_create(hVal *var);

void sbRef_set_ref(hRef ref, hVal *var);

hVal *sbRef_deref(hRef ref);
