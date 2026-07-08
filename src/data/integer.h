#include "common.h"

#include "data/value.h"

#define SARABANDE_INT_MAX ((1LL << 62) - 1)
#define SARABANDE_INT_MIN (-(1LL << 62) + 1)

void sbInteger_sys_init();

hInteger sbInteger_new(i64 value);

hInteger sbInteger_sum(hInteger a, hInteger b);

hInteger sbInteger_diff(hInteger a, hInteger b);

hInteger sbInteger_mul(hInteger a, hInteger b);

hInteger sbInteger_floordiv(hInteger a, hInteger b);

void sbInteger_method(hVm vm);
