#include "data/methods.h"

#include "vm/exec.h"
#include "data/symbol.h"
#include "data/list.h"

#define METHOD_IS(name) (!sbstrncmp(method_name, name, sizeof(name)))

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
      hV loop_func = sbVm_pop(vm);
      sbVm_pop(vm); /* remove method name */
      usize length;
      hV *iter_values = sbList_get_value(list.list, &length);
      for (usize i = 0; i < length; i++) {
        /* TODO : This doesn't work! It almost works, but when we call the
         * callback func, we need to, like, pause this function and run more
         * VM instructions, then come back to this one. How do? :thinking emoji: */
        printf("index %zu\n", i);
        sbVm_push(vm, iter_values[i]);
        sbVm_push(vm, HVINT(1));
        printf("call loop-func\n");
        sbVm_call_func(vm, loop_func);
        /* remove result of loop_func */
        printf("loop-func done\n");
        sbVm_pop(vm);
      }
    }
  } else {
    PANIC("method name to list is not symbol! (%lld)", method_name_val->type);
  }
}

