#include "common.h"

#include "lib/table.h"
#include "data/list.h"
#include "data/string.h"
#include "vm/exec.h"

sbCFuncStatus list_each_cfunc(hVm vm, flag init);
sbCFuncStatus list_map_cfunc(hVm vm, flag init);
sbCFuncStatus list_filter_cfunc(hVm vm, flag init);
sbCFuncStatus list_any_cfunc(hVm vm, flag init);
sbCFuncStatus list_all_cfunc(hVm vm, flag init);

sbLibTable g_list_methods;

static void length(hVm vm, hV *list, usize num_params) {
  if (num_params != 0) {
    PANIC("list#length takes no arguments!");
  }
  usize length;
  sbList_get_value(list->list, &length);
  sbVm_push_immediate(vm, &HVINT(length));
}

static void push(hVm vm, hV *list, usize num_params) {
  if (num_params != 1) {
    PANIC("list#push expects 1 argument!");
  }
  hV *to_append = sbVm_pop(vm);
  sbList_append(list->list, to_append);
  sbVm_push_immediate(vm, &HVNIL);
}

static void reverse(hVm vm, hV *list, usize num_params) {
  if (num_params != 0) {
    PANIC("list#reverse takes no arguments!");
  }
  /* TODO maybe mutate in place if no other refs */
  usize length;
  hV *elems = sbList_get_value(list->list, &length);
  hList new_list = sbList_new(length);
  for (usize i = length - 1; ; i--) {
    sbList_append(new_list, &elems[i]);
    if (i == 0) break;
  }
  sbList_get_value(new_list, &length);
  sbVm_push_immediate(vm, &HVLIST(new_list));
}

static void join(hVm vm, hV *list, usize num_params) {
  if (num_params > 1) {
    PANIC("list#join takes no arguments or 1 argument!");
  }
  flag join_with = FALSE;
  hString delimiter;
  if (num_params == 1) {
    hV *delimiter_v = sbVm_pop(vm);
    if (delimiter_v->type != IT_STRING) {
      PANIC("list#join parameter should be string!");
    }
    join_with = TRUE;
    delimiter = delimiter_v->string;
  }
  usize length;
  hV *elems = sbList_get_value(list->list, &length);
  hString joined = sbString_new("", 0);
  for (usize i = 0; i < length; i++) {
    if (elems[i].type != IT_STRING) {
      PANIC("need string to join");
    }
    joined = sbString_concat(joined, elems[i].string);
    if (join_with && i < length - 1) {
      joined = sbString_concat(joined, delimiter);
    }
  }
  sbVm_push_immediate(vm, &HVSTR(joined));
}

static void each(hVm vm, hV *list, usize num_params) {
  if (num_params != 1) {
    PANIC("wrong number of arguments passed to list#each");
  }
  sbVm_push_immediate(vm, list);
  sbVm_call_c_func(vm, list_each_cfunc);
}

static void map(hVm vm, hV *list, usize num_params) {
  if (num_params != 1) {
    PANIC("wrong number of arguments passed to list#map");
  }
  sbVm_push_immediate(vm, list);
  sbVm_call_c_func(vm, list_map_cfunc);
}

static void filter(hVm vm, hV *list, usize num_params) {
  if (num_params != 1) {
    PANIC("wrong number of arguments passed to list#filter");
  }
  sbVm_push_immediate(vm, list);
  sbVm_call_c_func(vm, list_filter_cfunc);
}

static void any_p(hVm vm, hV *list, usize num_params) {
  if (num_params != 1) {
    PANIC("wrong number of arguments passed to list#any?");
  }
  sbVm_push_immediate(vm, list);
  sbVm_call_c_func(vm, list_any_cfunc);
}

static void all_p(hVm vm, hV *list, usize num_params) {
  if (num_params != 1) {
    PANIC("wrong number of arguments passed to list#any?");
  }
  sbVm_push_immediate(vm, list);
  sbVm_call_c_func(vm, list_all_cfunc);
}

void sbList_create_methods(void) {
  sbLibTable_initialize(&g_list_methods, 16, TRUE);
  REGISTER_METHOD(&g_list_methods, "length", length);
  REGISTER_METHOD(&g_list_methods, "push", push);
  REGISTER_METHOD(&g_list_methods, "reverse", reverse);
  REGISTER_METHOD(&g_list_methods, "join", join);
  REGISTER_METHOD(&g_list_methods, "each", each);
  REGISTER_METHOD(&g_list_methods, "map", map);
  REGISTER_METHOD(&g_list_methods, "filter", filter);
  REGISTER_METHOD(&g_list_methods, "any?", any_p);
  REGISTER_METHOD(&g_list_methods, "all?", all_p);
}

/* --- */

sbCFuncStatus list_each_cfunc(hVm vm, flag init) {
  if (init) {
    /* store three state variables: the list we're iterating, the index into it, and the callback function */
    sbVm_request_var_space(vm, 3);
    hV *iterating_list = sbVm_pop(vm);
    hV *loop_func = sbVm_pop(vm);
    hV index = HVINT(0);
    vm->fp->locals[0] = *iterating_list;
    vm->fp->locals[1] = index;
    vm->fp->locals[2] = *loop_func;
  } else {
    /* loop func must have finished, so remove its result */
    sbVm_pop(vm);
  }

  usize current_index = vm->fp->locals[1].integer++;
  usize length;
  hV *iter_values = sbList_get_value(vm->fp->locals[0].list, &length);
  if (current_index < length) {
    sbVm_push(vm, &iter_values[current_index]);
    sbVm_push_immediate(vm, &HVINT(1));
    sbVm_call_func(vm, &vm->fp->locals[2]);
    return CFUNC_NEXT;
  } else {
    /* if index was past the end, callback function won't be called, and we'll exit. return nil */
    sbVm_push_immediate(vm, &HVNIL);
    return CFUNC_END;
  }
}

sbCFuncStatus list_map_cfunc(hVm vm, flag init) {
  if (init) {
    /* state: list being mapped, index, callback, result */
    sbVm_request_var_space(vm, 4);
    hV *iterating_list = sbVm_pop(vm);
    hV *map_func = sbVm_pop(vm);
    usize length;
    sbList_get_value(iterating_list->list, &length);
    hV index = HVINT(0);
    hV result = sbV_empty_list(length);

    vm->fp->locals[0] = *iterating_list;
    vm->fp->locals[1] = index;
    vm->fp->locals[2] = *map_func;
    vm->fp->locals[3] = result;
  } else {
    /* get result of map function and append its result to result list */
    hV *mapped = sbVm_pop(vm);
    sbList_append(vm->fp->locals[3].list, mapped);
  }

  usize current_index = vm->fp->locals[1].integer++;
  usize length;
  hV *iter_values = sbList_get_value(vm->fp->locals[0].list, &length);
  if (current_index < length) {
    sbVm_push(vm, &iter_values[current_index]);
    sbVm_push_immediate(vm, &HVINT(1));
    sbVm_call_func(vm, &vm->fp->locals[2]);
    return CFUNC_NEXT;
  } else {
    /* return result list */
    sbVm_push_immediate(vm, &vm->fp->locals[3]);
    return CFUNC_END;
  }
}

sbCFuncStatus list_filter_cfunc(hVm vm, flag init) {
  if (init) {
    /* state: list being filtered, index, callback, result */
    sbVm_request_var_space(vm, 4);
    hV *iterating_list = sbVm_pop(vm);
    hV *filter_func = sbVm_pop(vm);
    usize length;
    sbList_get_value(iterating_list->list, &length);
    hV index = HVINT(0);
    hV result = sbV_empty_list(length);

    vm->fp->locals[0] = *iterating_list;
    vm->fp->locals[1] = index;
    vm->fp->locals[2] = *filter_func;
    vm->fp->locals[3] = result;
  } else {
    /* get result of filter function and append element to result if true */
    hV *mapped = sbVm_pop(vm);
    hV *element = sbVm_pop(vm);
    if (!sbV_c_falsy(mapped)) {
      sbList_append(vm->fp->locals[3].list, element);
    }
  }

  usize current_index = vm->fp->locals[1].integer++;
  usize length;
  hV *iter_values = sbList_get_value(vm->fp->locals[0].list, &length);
  if (current_index < length) {
    /* put current value on stack twice: once as parameter for function, once to save for ourselves */
    sbVm_push(vm, &iter_values[current_index]);
    sbVm_push(vm, &iter_values[current_index]);
    sbVm_push_immediate(vm, &HVINT(1));
    sbVm_call_func(vm, &vm->fp->locals[2]);
    return CFUNC_NEXT;
  } else {
    /* return result list */
    sbVm_push_immediate(vm, &vm->fp->locals[3]);
    return CFUNC_END;
  }
}

sbCFuncStatus list_any_cfunc(hVm vm, flag init) {
  if (init) {
    /* state: list being filtered, index, callback */
    sbVm_request_var_space(vm, 3);
    hV *iterating_list = sbVm_pop(vm);
    hV *pred_func = sbVm_pop(vm);
    hV index = HVINT(0);

    vm->fp->locals[0] = *iterating_list;
    vm->fp->locals[1] = index;
    vm->fp->locals[2] = *pred_func;
  } else {
    /* get result of predicate function */
    hV *mapped = sbVm_pop(vm);
    if (!sbV_c_falsy(mapped)) {
      /* one was true! return true */
      sbVm_push_immediate(vm, &HVBOOL(TRUE));
      return CFUNC_END;
    }
  }

  /* haven't found any yet... */
  usize current_index = vm->fp->locals[1].integer++;
  usize length;
  hV *iter_values = sbList_get_value(vm->fp->locals[0].list, &length);
  if (current_index < length) {
    /* try next element */
    sbVm_push(vm, &iter_values[current_index]);
    sbVm_push_immediate(vm, &HVINT(1));
    sbVm_call_func(vm, &vm->fp->locals[2]);
    return CFUNC_NEXT;
  } else {
    /* no result found */
    sbVm_push_immediate(vm, &HVBOOL(FALSE));
    return CFUNC_END;
  }
}

sbCFuncStatus list_all_cfunc(hVm vm, flag init) {
  if (init) {
    /* state: list being filtered, index, callback */
    sbVm_request_var_space(vm, 3);
    hV *iterating_list = sbVm_pop(vm);
    hV *pred_func = sbVm_pop(vm);
    hV index = HVINT(0);

    vm->fp->locals[0] = *iterating_list;
    vm->fp->locals[1] = index;
    vm->fp->locals[2] = *pred_func;
  } else {
    /* get result of predicate function */
    hV *mapped = sbVm_pop(vm);
    if (sbV_c_falsy(mapped)) {
      /* one was false; return false */
      sbVm_push_immediate(vm, &HVBOOL(FALSE));
      return CFUNC_END;
    }
  }

  /* all true so far */
  usize current_index = vm->fp->locals[1].integer++;
  usize length;
  hV *iter_values = sbList_get_value(vm->fp->locals[0].list, &length);
  if (current_index < length) {
    /* try next element */
    sbVm_push(vm, &iter_values[current_index]);
    sbVm_push_immediate(vm, &HVINT(1));
    sbVm_call_func(vm, &vm->fp->locals[2]);
    return CFUNC_NEXT;
  } else {
    /* all passed */
    sbVm_push_immediate(vm, &HVBOOL(TRUE));
    return CFUNC_END;
  }
}
