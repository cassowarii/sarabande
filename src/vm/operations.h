#include "common.h"

void sbV_message_handler(hVm vm);

hV sbV_add(const hV *a, const hV *b);

hV sbV_sub(const hV *a, const hV *b);

hV sbV_mul(const hV *a, const hV *b);

hV sbV_floordiv(const hV *a, const hV *b);

hV sbV_mod(const hV *a, const hV *b);

void sbV_incr(hV *a);

void sbV_decr(hV *a);

hV sbV_eq(const hV *a, const hV *b);

hV sbV_lt(const hV *a, const hV *b);

hV sbV_le(const hV *a, const hV *b);

hV sbV_append(hV *a, hV *b);

void sbV_index(hVm vm);

hV sbV_rangeindex(hV *a, hV *b, hV *c);

void sbV_index_set(hV *obj, hV *key, hV *value);
