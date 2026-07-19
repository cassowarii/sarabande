#include "common.h"

void sbV_message_handler(hVm vm);

hVal sbV_add(const hVal *a, const hVal *b);

hVal sbV_sub(const hVal *a, const hVal *b);

hVal sbV_mul(const hVal *a, const hVal *b);

hVal sbV_floordiv(const hVal *a, const hVal *b);

hVal sbV_mod(const hVal *a, const hVal *b);

void sbV_incr(hVal *a);

void sbV_decr(hVal *a);

hVal sbV_eq(const hVal *a, const hVal *b);

hVal sbV_lt(const hVal *a, const hVal *b);

hVal sbV_le(const hVal *a, const hVal *b);

hVal sbV_append(hVal *a, hVal *b);

void sbV_index_value(hVm vm);

hVal sbV_rangeindex(hVal *a, hVal *b, hVal *c);

void sbV_index_set(hVal *obj, hVal *key, hVal *value);

void sbV_deref(hVm vm);

void sbV_refput(hVm vm);
