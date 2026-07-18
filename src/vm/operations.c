#include "vm/operations.h"

#include "vm/exec.h"
#include "data/integer.h"
#include "data/list.h"
#include "data/hashtable.h"
#include "data/string.h"
#include "lib/table.h"
#include "lib/sentinel.h"

hVal sbV_add(const hVal *a, const hVal *b) {
  if (a->type == IT_INTEGER && b->type == IT_INTEGER) {
    return HVINT(sbInteger_sum(a->integer, b->integer));
  } else {
    PANIC("todo (add type %lld and type %lld)", (long long)a->type, (long long)b->type);
  }
}

hVal sbV_sub(const hVal *a, const hVal *b) {
  if (a->type == IT_INTEGER && b->type == IT_INTEGER) {
    return HVINT(sbInteger_diff(a->integer, b->integer));
  } else {
    PANIC("todo (subtract type %llu minus %llu)", (long long)a->type, (long long)b->type);
  }
}

hVal sbV_mul(const hVal *a, const hVal *b) {
  if (a->type == IT_INTEGER && b->type == IT_INTEGER) {
    return HVINT(sbInteger_mul(a->integer, b->integer));
  } else {
    PANIC("todo");
  }
}

hVal sbV_floordiv(const hVal *a, const hVal *b) {
  if (a->type == IT_INTEGER && b->type == IT_INTEGER) {
    return HVINT(sbInteger_floordiv(a->integer, b->integer));
  } else {
    PANIC("todo");
  }
}

hVal sbV_mod(const hVal *a, const hVal *b) {
  if (a->type == IT_INTEGER && b->type == IT_INTEGER) {
    return HVINT(a->integer % b->integer);
  } else {
    PANIC("todo %lld", (long long)a->type);
  }
}

void sbV_incr(hVal *a) {
  if (a->type == IT_INTEGER) {
    a->integer += 1;
  } else {
    PANIC("todo");
  }
}

void sbV_decr(hVal *a) {
  if (a->type == IT_INTEGER) {
    a->integer -= 1;
  } else {
    PANIC("todo");
  }
}

hVal sbV_eq(const hVal *a, const hVal *b) {
  if (sbV_c_eq(a, b)) {
    return HVBOOL(TRUE);
  } else {
    return HVBOOL(FALSE);
  }
}

hVal sbV_lt(const hVal *a, const hVal *b) {
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

hVal sbV_le(const hVal *a, const hVal *b) {
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

hVal sbV_append(hVal *a, hVal *b) {
  if (a->type == IT_LIST) {
    sbList_append(a->list, b);
    return HVNIL;
  } else {
    PANIC("todo");
  }
}

void sbV_index_value(hVm vm) {
  hVal *a = sbVm_peek(vm, 1);
  hVal *b = sbVm_peek(vm, 0);
  if (a->type == IT_LIST && b->type == IT_INTEGER) {
    sbVm_npop(vm, 2);
    hVal result = sbList_index_value(a->list, b->integer);
    sbVm_push(vm, &result);
  } else if (a->type == IT_MODULE && b->type == IT_SYMBOL) {
    sbVm_npop(vm, 2);
    hVal *result = sbLibTable_find_value(a->module, b->symbol);
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

hVal sbV_rangeindex(hVal *a, hVal *b, hVal *c) {
  if (b->type == IT_INTEGER && c->type == IT_INTEGER) {
    usize length;
    i64 min = sbInteger_get_value(b->integer);
    i64 max = sbInteger_get_value(c->integer);
    if (a->type == IT_LIST) {
      sbVar *elements = sbList_get_value(a->list, &length);
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

void sbV_index_set(hVal *obj, hVal *key, hVal *value) {
  if (obj->type == IT_LIST) {
    PANIC("todo");
  } else if (obj->type == IT_HASH) {
    sbHash_insert(obj->hash, key, value);
  } else {
    PANIC("todo %lld", (long long)obj->type);
  }
}
