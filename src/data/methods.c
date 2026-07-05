#include "data/methods.h"

#include "vm/exec.h"
#include "data/symbol.h"
#include "data/list.h"

#define METHOD_IS(name) (!sbstrncmp(method_name, name, sizeof(name)))

void list_each_cfunc(hVm vm, flag init);
void list_map_cfunc(hVm vm, flag init);

void sbList_method(hV list, hVm vm) {
  if (list.type != IT_LIST) {
    CHECK("can't call sbList_method on something that isn't a list");
  }
  hV argc = sbVm_pop(vm);
  if (argc.type != IT_INTEGER) {
    CHECK("argc of send should be integer!");
  }
  /* subtract 1 because the method name is itself a param */
  usize num_params = argc.integer - 1;
  hV *method_name_val = sbVm_peek(vm, num_params);
  if (method_name_val->type == IT_SYMBOL) {
    const char *method_name = sbSymbol_name(method_name_val->symbol);
    printf("method name: %s\n", method_name);
    if (METHOD_IS("each")) {
      if (num_params != 1) {
        PANIC("wrong number of arguments passed to list#each!");
      }
      sbVm_push(vm, list);
      sbVm_call_c_func(vm, list_each_cfunc);
    }
  } else {
    PANIC("method name to list is not symbol! (%ld)", method_name_val->type);
  }
}

void list_each_cfunc(hVm vm, flag init) {
  if (init) {
    /* store three state variables: the list we're iterating, the index into it, and the callback function */
    sbVm_request_var_space(vm, 3);
    hV iterating_list = sbVm_pop(vm);
    hV loop_func = sbVm_pop(vm);
    sbVm_pop(vm); /* remove method name */
    hV index = HVINT(0);
    vm->fp->locals[0] = iterating_list;
    vm->fp->locals[1] = index;
    vm->fp->locals[2] = loop_func;
  } else {
    /* loop func must have finished, so remove its result */
    sbVm_pop(vm);
  }

  usize current_index = vm->fp->locals[1].integer++;
  usize length;
  hV *iter_values = sbList_get_value(vm->fp->locals[0].list, &length);
  if (current_index < length) {
    sbVm_push(vm, iter_values[current_index]);
    sbVm_push(vm, HVINT(1));
    sbVm_call_func(vm, vm->fp->locals[2]);
  } else {
    /* if index was past the end, callback function won't be called, and we'll exit. return nil */
    sbVm_push(vm, HVNIL);
  }
}
