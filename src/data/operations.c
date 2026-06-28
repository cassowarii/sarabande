#include "data/operations.h"

#include "data/integer.h"

hV sbV_add(const hV *a, const hV *b) {
  if (a->type == IT_INTEGER && b->type == IT_INTEGER) {
    return sbV_int(sbInteger_sum(a->integer, b->integer));
  } else {
    PANIC("todo");
  }
}

hV sbV_sub(const hV *a, const hV *b) {
  if (a->type == IT_INTEGER && b->type == IT_INTEGER) {
    return sbV_int(sbInteger_diff(a->integer, b->integer));
  } else {
    PANIC("todo");
  }
}

hV sbV_mul(const hV *a, const hV *b) {
  if (a->type == IT_INTEGER && b->type == IT_INTEGER) {
    return sbV_int(sbInteger_mul(a->integer, b->integer));
  } else {
    PANIC("todo");
  }
}

hV sbV_floordiv(const hV *a, const hV *b) {
  if (a->type == IT_INTEGER && b->type == IT_INTEGER) {
    return sbV_int(sbInteger_floordiv(a->integer, b->integer));
  } else {
    PANIC("todo");
  }
}

hV sbV_incr(const hV *a) {
  if (a->type == IT_INTEGER) {
    return sbV_int(a->integer + 1);
  } else {
    PANIC("todo");
  }
}

hV sbV_decr(const hV *a) {
  if (a->type == IT_INTEGER) {
    return sbV_int(a->integer - 1);
  } else {
    PANIC("todo");
  }
}

hV sbV_eq(const hV *a, const hV *b) {
  if (sbV_c_eq(a, b)) {
    return HVBOOL(TRUE);
  } else {
    return HVBOOL(FALSE);
  }
}
