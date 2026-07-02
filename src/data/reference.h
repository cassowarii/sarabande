#include "common.h"

void sbRef_sys_init();

void sbRef_sys_deinit();

hRef sbRef_create(hV *var);

void sbRef_set_ref(hRef ref, hV *var);

hV *sbRef_deref(hRef ref);
