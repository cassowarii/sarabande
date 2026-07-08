#include "data/list.h"

#include "gc/gcinfo.h"
#include "vm/exec.h"
#include "data/symbol.h"
#include "data/string.h"

#define LIST_PER_BLOCK 256
#define METHOD_IS(name) (!sbstrncmp(method_name, name, sizeof(name)))

void list_each_cfunc(hVm vm, flag init);
void list_map_cfunc(hVm vm, flag init);
void list_filter_cfunc(hVm vm, flag init);
void list_any_cfunc(hVm vm, flag init);
void list_all_cfunc(hVm vm, flag init);

/* TODO etc etc */
sbPool g_list_pool;

typedef struct sbList {
  GCINFO gc;
  u64 handle;
  sbBuffer items;
} sbList;

sbList *get_list_by_handle(hList handle);

void sbList_sys_init() {
  sbPool_initialize(&g_list_pool, sizeof(sbList), LIST_PER_BLOCK);
}

void sbList_sys_deinit() {
  //sbPool_deinitialize(&g_list_pool);
}

hList sbList_new(usize capacity) {
  usize index;
  sbList *l = sbPool_alloc(&g_list_pool, &index);
  if (capacity < 4) capacity = 4;
  sbBuffer_initialize(&l->items, capacity * sizeof(hV));
  return index;
}

void sbList_append(hList list, hV *item) {
  sbList *l = get_list_by_handle(list);
  sbV_retain(item);
  sbBuffer_append(&l->items, item, sizeof(hV));
}

hV *sbList_get_value(hList list, usize *length) {
  sbList *l = get_list_by_handle(list);
  if (length) *length = l->items.size / sizeof(hV);
  return (hV*)l->items.data;
}

hV *sbList_index(hList list, usize index) {
  sbList *l = get_list_by_handle(list);
  hV *item = &((hV*)l->items.data)[index];
  return item;
}

void sbList_method(hVm vm) {
  hV *list = sbVm_pop(vm);
  if (list->type != IT_LIST) {
    CHECK("can't call sbList_method on something that isn't a list");
  }
  hV *argc = sbVm_pop(vm);
  if (argc->type != IT_INTEGER) {
    CHECK("argc of send should be integer!");
  }
  /* subtract 1 because the method name is itself a param */
  usize num_params = argc->integer - 1;
  hV *method_name_val = sbVm_peek(vm, num_params);
  if (method_name_val->type != IT_SYMBOL) {
    /* TODO this may become not true */
    PANIC("method name for list must be symbol!");
  }

  const char *method_name = sbSymbol_name(method_name_val->symbol);
  /* TODO: Need a better way of resolving these */
  if (METHOD_IS("length")) {
    if (num_params != 0) {
      PANIC("list#length takes no arguments!");
    }
    sbVm_pop(vm); /* remove method name */
    usize length;
    sbList_get_value(list->list, &length);
    sbVm_push_immediate(vm, &HVINT(length));
  } else if (METHOD_IS("push")) {
    if (num_params != 1) {
      PANIC("list#push expects 1 argument!");
    }
    hV *to_append = sbVm_pop(vm);
    sbVm_pop(vm); /* remove method name */
    sbList_append(list->list, to_append);
    sbVm_push_immediate(vm, &HVNIL);
  } else if (METHOD_IS("each")) {
    if (num_params != 1) {
      PANIC("wrong number of arguments passed to list#each");
    }
    sbVm_push_immediate(vm, list);
    sbVm_call_c_func(vm, list_each_cfunc);
  } else if (METHOD_IS("map")) {
    if (num_params != 1) {
      PANIC("wrong number of arguments passed to list#map");
    }
    sbVm_push_immediate(vm, list);
    sbVm_call_c_func(vm, list_map_cfunc);
  } else if (METHOD_IS("filter")) {
    if (num_params != 1) {
      PANIC("wrong number of arguments passed to list#filter");
    }
    sbVm_push_immediate(vm, list);
    sbVm_call_c_func(vm, list_filter_cfunc);
  } else if (METHOD_IS("any?")) {
    if (num_params != 1) {
      PANIC("wrong number of arguments passed to list#any?");
    }
    sbVm_push_immediate(vm, list);
    sbVm_call_c_func(vm, list_any_cfunc);
  } else if (METHOD_IS("all?")) {
    if (num_params != 1) {
      PANIC("wrong number of arguments passed to list#any?");
    }
    sbVm_push_immediate(vm, list);
    sbVm_call_c_func(vm, list_all_cfunc);
  } else if (METHOD_IS("reverse")) {
    if (num_params != 0) {
      PANIC("list#reverse takes no arguments!");
    }
    /* TODO maybe mutate in place if no other refs */
    sbVm_pop(vm); /* remove method name */
    usize length;
    hV *elems = sbList_get_value(list->list, &length);
    hList new_list = sbList_new(length);
    for (usize i = length - 1; ; i--) {
      sbList_append(new_list, &elems[i]);
      if (i == 0) break;
    }
    sbList_get_value(new_list, &length);
    sbVm_push_immediate(vm, &HVLIST(new_list));
  } else if (METHOD_IS("join")) {
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
    sbVm_pop(vm); /* remove method name */
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
  } else {
    PANIC("unknown method name for list");
  }
}

/* --- */

sbList *get_list_by_handle(hList handle) {
  return sbPool_get_entry(&g_list_pool, handle);
}

void list_each_cfunc(hVm vm, flag init) {
  if (init) {
    /* store three state variables: the list we're iterating, the index into it, and the callback function */
    sbVm_request_var_space(vm, 3);
    hV *iterating_list = sbVm_pop(vm);
    hV *loop_func = sbVm_pop(vm);
    sbVm_pop(vm); /* remove method name */
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
  } else {
    /* if index was past the end, callback function won't be called, and we'll exit. return nil */
    sbVm_push_immediate(vm, &HVNIL);
  }
}

void list_map_cfunc(hVm vm, flag init) {
  if (init) {
    /* state: list being mapped, index, callback, result */
    sbVm_request_var_space(vm, 4);
    hV *iterating_list = sbVm_pop(vm);
    hV *map_func = sbVm_pop(vm);
    sbVm_pop(vm); /* remove method name */
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
  } else {
    /* return result list */
    sbVm_push_immediate(vm, &vm->fp->locals[3]);
  }
}

void list_filter_cfunc(hVm vm, flag init) {
  if (init) {
    /* state: list being filtered, index, callback, result */
    sbVm_request_var_space(vm, 4);
    hV *iterating_list = sbVm_pop(vm);
    hV *filter_func = sbVm_pop(vm);
    sbVm_pop(vm); /* remove method name */
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
  } else {
    /* return result list */
    sbVm_push_immediate(vm, &vm->fp->locals[3]);
  }
}

void list_any_cfunc(hVm vm, flag init) {
  if (init) {
    /* state: list being filtered, index, callback */
    sbVm_request_var_space(vm, 3);
    hV *iterating_list = sbVm_pop(vm);
    hV *pred_func = sbVm_pop(vm);
    sbVm_pop(vm); /* remove method name */
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
      return;
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
  } else {
    /* no result found */
    sbVm_push_immediate(vm, &HVBOOL(FALSE));
  }
}

void list_all_cfunc(hVm vm, flag init) {
  if (init) {
    /* state: list being filtered, index, callback */
    sbVm_request_var_space(vm, 3);
    hV *iterating_list = sbVm_pop(vm);
    hV *pred_func = sbVm_pop(vm);
    sbVm_pop(vm); /* remove method name */
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
      return;
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
  } else {
    /* all passed */
    sbVm_push_immediate(vm, &HVBOOL(TRUE));
  }
}
