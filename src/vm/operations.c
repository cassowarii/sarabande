#include "vm/operations.h"

#include "vm/exec.h"
#include "data/integer.h"
#include "data/list.h"
#include "data/hashtable.h"
#include "data/string.h"
#include "lib/table.h"
#include "lib/sentinel.h"

hV sbV_add(const hV *a, const hV *b) {
  if (a->type == IT_INTEGER && b->type == IT_INTEGER) {
    return sbV_int(sbInteger_sum(a->integer, b->integer));
  } else {
    PANIC("todo (add type %lld and type %lld)", (long long)a->type, (long long)b->type);
  }
}

hV sbV_sub(const hV *a, const hV *b) {
  if (a->type == IT_INTEGER && b->type == IT_INTEGER) {
    return sbV_int(sbInteger_diff(a->integer, b->integer));
  } else {
    PANIC("todo (subtract type %llu minus %llu)", (long long)a->type, (long long)b->type);
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

hV sbV_mod(const hV *a, const hV *b) {
  if (a->type == IT_INTEGER && b->type == IT_INTEGER) {
    return sbV_int(a->integer % b->integer);
  } else {
    PANIC("todo %lld", (long long)a->type);
  }
}

void sbV_incr(hV *a) {
  if (a->type == IT_INTEGER) {
    a->integer += 1;
  } else {
    PANIC("todo");
  }
}

void sbV_decr(hV *a) {
  if (a->type == IT_INTEGER) {
    a->integer -= 1;
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

hV sbV_lt(const hV *a, const hV *b) {
  if (a->type == IT_INTEGER && b->type == IT_INTEGER) {
    if (a->integer < b->integer) {
      return HVBOOL(TRUE);
    } else {
      return HVBOOL(FALSE);
    }
  } else if (a->type == IT_FLOAT || b->type == IT_FLOAT) {
    if (a->type == IT_INTEGER) {
      /* integer < float */
      return HVBOOL(a->integer < (double)b->float_val);
    } else if (b->type == IT_INTEGER) {
      /* float < integer */
      return HVBOOL(a->float_val < (double)b->integer);
    } else {
      /* two floats */
      return HVBOOL(a->float_val < b->float_val);
    }
  } else {
    PANIC("todo %lld", (long long)a->type);
  }
}

hV sbV_le(const hV *a, const hV *b) {
  if (a->type == IT_INTEGER && b->type == IT_INTEGER) {
    if (a->integer <= b->integer) {
      return HVBOOL(TRUE);
    } else {
      return HVBOOL(FALSE);
    }
  } else if (a->type == IT_FLOAT || b->type == IT_FLOAT) {
    if (a->type == IT_INTEGER) {
      /* integer <= float */
      return HVBOOL(a->integer <= (double)b->float_val);
    } else if (b->type == IT_INTEGER) {
      /* float <= integer */
      return HVBOOL(a->float_val <= (double)b->integer);
    } else {
      /* two floats */
      return HVBOOL(a->float_val <= b->float_val);
    }
  } else {
    PANIC("todo");
  }
}

hV sbV_append(hV *a, hV *b) {
  if (a->type == IT_LIST) {
    sbList_append(a->list, b);
    return HVNIL;
  } else {
    PANIC("todo");
  }
}

void sbV_index(hVm vm) {
  hV *a = sbVm_peek(vm, 1);
  hV *b = sbVm_peek(vm, 0);
  if (a->type == IT_LIST && b->type == IT_INTEGER) {
    sbVm_npop(vm, 2);
    hV *result = sbList_index(a->list, b->integer);
    sbVm_push(vm, result);
  } else if (a->type == IT_MODULE && b->type == IT_SYMBOL) {
    sbVm_npop(vm, 2);
    hV *result = sbLibTable_find_value(a->module, b->symbol);
    sbVm_push(vm, result);
  } else {
    sbVm_swap(vm);                                      /* b a */
    sbVm_push_immediate(vm, &HVSYM(S_OP_INDEX));        /* b a op::index */
    sbVm_swap(vm);                                      /* b op::index a */
    sbVm_push_immediate(vm, &HVINT(2));                 /* b op::index a 2 */
    sbVm_swap(vm);                                      /* b op::index 2 a */
    sbLib_resolve_method(vm);
  }
}

hV sbV_rangeindex(hV *a, hV *b, hV *c) {
  if (b->type == IT_INTEGER && c->type == IT_INTEGER) {
    usize length;
    i64 min = sbInteger_get_value(b->integer);
    i64 max = sbInteger_get_value(c->integer);
    if (a->type == IT_LIST) {
      hV *elements = sbList_get_value(a->list, &length);
      if (min >= max || min >= length || max < 0) {
        /* backwards or out of range */
        return sbV_empty_list(0);
      } else {
        /* now we know 0 <= max, min < max, min < length. clip min and max to bounds */
        if (max >= length) max = length;
        if (min < 0) min = 0;
        return HVLIST(sbList_of(max - min, &elements[min]));
      }
    } else if (a->type == IT_STRING) {
      /* substring operation */
      char scratch[8];
      const char *strdata = sbString_get_value(a->string, scratch, &length);
      if (min >= max || min >= length || max < 0) {
        /* backwards or out of range */
        return sbV_empty_string();
      } else {
        /* now we know 0 <= max, min < max, min < length. clip min and max to bounds */
        if (max >= length) max = length;
        if (min < 0) min = 0;
        return HVSTR(sbString_new(&strdata[min], max - min));
      }
    } else {
      PANIC("todo %lld %lld %lld", (long long)a->type, (long long)b->type, (long long)c->type);
    }
  } else {
    PANIC("Only integers are supported for range indexing!");
  }
}

void sbV_index_set(hV *obj, hV *key, hV *value) {
  if (obj->type == IT_LIST) {
    PANIC("todo");
  } else if (obj->type == IT_HASH) {
    sbHash_insert(obj->hash, key, value);
  } else {
    PANIC("todo %lld", (long long)obj->type);
  }
}
