#include "common.h"
#include "data/symbol.h"
#include "vm/exec.h"

#include "lib/table.h"
#include "lib/sentinel.h"
#include "lib/method.h"
#include "lib/module.h"

sbCFuncStatus please_call_again(hVm vm, flag init);

void sbLib_sys_init() {
  sbLib_create_sentinels();

  sbLib_loadmodule_global();

  sbList_create_methods();
  sbString_create_methods();
  sbInteger_create_methods();
  sbFloat_create_methods();
}

void sbLib_sys_deinit() {
  sbLibTable_deinitialize(&g_list_methods);
  sbLibTable_deinitialize(&g_string_methods);
  sbLibTable_deinitialize(&g_integer_methods);
  sbLibTable_deinitialize(&g_float_methods);

  sbLibTable_deinitialize(&g_global_module);
}

void sbLib_resolve_method(hVm vm) {
  hV *target = sbVm_pop(vm);
  hV *argc = sbVm_pop(vm);
  if (argc->type != IT_INTEGER) {
    CHECK("argc of send should be integer!");
  }
  hV *method_name_val = sbVm_pop(vm);
  if (method_name_val->type != IT_SYMBOL) {
    /* TODO this may become not true */
    PANIC("method name must be symbol!");
  }
  hSymbol method_sym = method_name_val->symbol;

  /* subtract 1 because the method name is itself a param */
  usize num_params = argc->integer - 1;
  hLibTable table_to_use = NULL;
  switch(target->type) {
    case IT_LIST:
      table_to_use = &g_list_methods;
      break;
    case IT_STRING:
      table_to_use = &g_string_methods;
      break;
    case IT_INTEGER:
      table_to_use = &g_integer_methods;
      break;
    case IT_FLOAT:
      table_to_use = &g_float_methods;
      break;
  }

  if (table_to_use == NULL) {
    /* did not find built in method table */
    if (target->type <= 0) {
      /* for built in type, just fail, it's not done yet */
      PANIC("Have not implemented this method table yet! (%lld)", (long long)target->type);
    } else {
      /* for a user defined type, try to retrieve its method: we'll need to jump back into VM for this */
      /* convert stack position ...params :methodname (argc+1) target --> ...params argc :methodname 1 target,
       * and then call twice */
      sbVm_push_immediate(vm, &HVINT(num_params));
      sbVm_push_immediate(vm, &HVSYM(method_sym));
      sbVm_push_immediate(vm, target); /* if target points to xstack, need to push before it gets overwritten */
      sbVm_push_immediate(vm, &HVINT(1));
      sbVm_swap(vm); /* now put target on top of stack */
      sbVm_call_c_func(vm, please_call_again);
    }
  } else {
    /* built in type that has a method table defined: find method */
    sbLibMethod f = sbLibTable_find_method(table_to_use, method_name_val->symbol);
    if (f) {
      f(vm, target, num_params);
    } else {
      PANIC("invalid method name for type %lld: %s", (long long)target->type, sbSymbol_name(method_name_val->symbol));
    }
  }
}

void sbLib_resolve_global(hVm vm) {
  hV *target = sbVm_pop(vm);
  if (target->type != IT_SYMBOL) {
    CHECK("global lookup must be symbol!");
  }
  hV *v = sbLibTable_find_value(&g_global_module, target->symbol);
  if (v) {
    sbVm_push_immediate(vm, v);
  } else {
    PANIC("name '%s' is not defined", sbSymbol_name(target->symbol));
  }
}

/* --- */

/* function that resolves methods from BC_SEND for non intrinsic types (aka functions) */
/* we have something like a.b(c,d,e), we need to call twice to get result of a(:b)(c,d,e) */
/* resolve_method should have set us up so we can just call in twice */
sbCFuncStatus please_call_again(hVm vm, flag init) {
  hV *target = sbVm_pop(vm);
  if (target->type <= 0) {
    PANIC("i have not implemented partial methods for builtin types yet! i'll get to it");
  }

  if (init) {
    /* first time around: call to dispatch method */
    sbVm_call_func(vm, target);

    /* please call us again if you have any questions*/
    return CFUNC_NEXT;
  } else {
    /* second time around: call method on its parameters */
    sbVm_transfer_to_func(vm, target);

    /* please forget we ever existed */
    return CFUNC_TAILCALL;
  }
}
