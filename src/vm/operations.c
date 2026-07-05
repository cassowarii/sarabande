#include "vm/operations.h"

#include "vm/exec.h"
#include "data/integer.h"
#include "data/list.h"
#include "data/hashtable.h"
#include "data/methods.h"

void sbV_message_handler(hVm vm) {
  hV target = sbVm_pop(vm);
  switch(target.type) {
    case IT_LIST:
      sbList_method(target, vm);
      break;
    default:
      if (target.type < 0) {
        PANIC("haven't implemented method calling for this intrinsic type!");
      }
      if (target.type & FLAG_SQUIGGLY) {
        /* squiggle function */
        sbVm_call_func(vm, target);
      } else {
        /* normal function */
        PANIC("haven't implemented method calling for functions yet!");
        //sbFunction_method(target, vm);
      }
  }
}

hV sbV_add(const hV *a, const hV *b) {
  if (a->type == IT_INTEGER && b->type == IT_INTEGER) {
    return sbV_int(sbInteger_sum(a->integer, b->integer));
  } else {
    PANIC("todo (add type %llu and type %llu)", (long long)a->type, (long long)b->type);
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

hV sbV_lt(const hV *a, const hV *b) {
  if (a->type == IT_INTEGER && b->type == IT_INTEGER) {
    if (a->integer < b->integer) {
      return HVBOOL(TRUE);
    } else {
      return HVBOOL(FALSE);
    }
  } else {
    PANIC("todo");
  }
}

hV sbV_le(const hV *a, const hV *b) {
  if (a->type == IT_INTEGER && b->type == IT_INTEGER) {
    if (a->integer <= b->integer) {
      return HVBOOL(TRUE);
    } else {
      return HVBOOL(FALSE);
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

hV sbV_index(hV *a, hV *b) {
  if (a->type == IT_LIST && b->type == IT_INTEGER) {
    return sbList_index(a->list, b->integer);
  } else {
    PANIC("todo %lld %lld", (long long)a->type, (long long)b->type);
  }
}

hV sbV_scope_get(hV *obj, hV *key) {
  if (obj->type == IT_HASH) {
    return sbHash_find(obj->hash, key);
  } else {
    PANIC("todo %lld", (long long)obj->type);
  }
}

void sbV_scope_set(hV *obj, hV *key, hV *value) {
  if (obj->type == IT_HASH) {
    sbHash_insert(obj->hash, key, value);
  } else {
    PANIC("todo %lld", (long long)obj->type);
  }
}
