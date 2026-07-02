#include "common.h"

void sbClosure_sys_init();

void sbClosure_sys_deinit();

hClosure sbClosure_create(usize num_vars);

void sbClosure_set_var(hClosure which, usize index, hV *var);

void sbClosure_set_ref(hClosure which, usize index, hV *what);

hV *sbClosure_get_var(hClosure which, usize index);

hV sbClosure_get_ref(hClosure which, usize index);
