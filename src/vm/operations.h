#include "common.h"

void sbV_message_handler(hVm vm);

hV sbV_add(const hV *a, const hV *b);

hV sbV_sub(const hV *a, const hV *b);

hV sbV_mul(const hV *a, const hV *b);

hV sbV_floordiv(const hV *a, const hV *b);

hV sbV_incr(const hV *a);

hV sbV_decr(const hV *a);

hV sbV_eq(const hV *a, const hV *b);

hV sbV_lt(const hV *a, const hV *b);

hV sbV_le(const hV *a, const hV *b);

hV sbV_append(hV *a, hV *b);

hV sbV_index(hV *a, hV *b);

hV sbV_scope_get(hV *hash, hV *key);

void sbV_scope_set(hV *hash, hV *key, hV *value);
