#include "common.h"

void sbClosure_sys_init();

void sbClosure_sys_deinit();

hClosure sbClosure_create(usize num_vars);

void sbClosure_set_var(hClosure which, usize index, hVal *var);

void sbClosure_set_ref(hClosure which, usize index, hVal *what);

hVal *sbClosure_get_var(hClosure which, usize index);

hVal sbClosure_get_ref(hClosure which, usize index);
