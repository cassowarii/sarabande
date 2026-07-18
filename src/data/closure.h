#include "common.h"

void sbClosure_sys_init();

void sbClosure_sys_deinit();

hClosure sbClosure_create(usize num_vars);

void sbClosure_set_value(hClosure which, usize index, hVal *var);

void sbClosure_set_var(hClosure which, usize index, hVar what);

hVal sbClosure_get_value(hClosure which, usize index);

hVar sbClosure_get_var(hClosure which, usize index);
