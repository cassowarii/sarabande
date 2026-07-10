#include "common.h"

#include "data/value.h"

#define SARABANDE_INT_MAX ((1LL << 48) - 1)
#define SARABANDE_INT_MIN (-(1LL << 48) + 1)

void sbInteger_sys_init();

void sbInteger_sys_deinit();

hInteger sbInteger_new(i64 value);

hInteger sbInteger_sum(hInteger a, hInteger b);

hInteger sbInteger_diff(hInteger a, hInteger b);

hInteger sbInteger_mul(hInteger a, hInteger b);

hInteger sbInteger_floordiv(hInteger a, hInteger b);

void sbInteger_fprint(FILE *out, hInteger a);

usize sbInteger_snprint(char *buf, usize size, hInteger a);

double sbInteger_as_double(hInteger a);

void sbInteger_method(hVm vm);
