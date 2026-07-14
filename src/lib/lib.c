#include "common.h"
#include "vm/exec.h"

#include "data/symbol.h"
#include "data/reference.h"

#include "lib/table.h"
#include "lib/sentinel.h"
#include "lib/method.h"
#include "lib/module.h"

static sbLibTable *find_method_table(hV *target);
static sbCFuncStatus please_call_again(hVm vm, flag init);

void sbLib_sys_init() {
  sbLib_create_sentinels();

  sbLib_loadmodule_global();

  sbList_create_methods();
  sbString_create_methods();
  sbInteger_create_methods();
  sbFloat_create_methods();
  sbHash_create_methods();
}

void sbLib_sys_deinit() {
  sbLibTable_deinitialize(&g_list_methods);
  sbLibTable_deinitialize(&g_string_methods);
  sbLibTable_deinitialize(&g_integer_methods);
  sbLibTable_deinitialize(&g_float_methods);
  sbLibTable_deinitialize(&g_hash_methods);

  sbLibTable_deinitialize(&g_global_module);
}

void sbLib_resolve_method(hVm vm) {
  hV *target = sbVm_pop(vm);
  hV *argc = sbVm_pop(vm);
  if (argc->type != IT_INTEGER) {
    CHECK("argc of send should be integer!");
  }
  if (argc->integer == 0) {
    PANIC("Method selection is required!");
  }
  /* subtract 1 because the method name is itself a param */
  usize num_params = argc->integer - 1;

  hV *method_name_val = sbVm_pop(vm);
  if (method_name_val->type != IT_SYMBOL) {
    PANIC("method name must be symbol!");
  }
  hSymbol method_sym = method_name_val->symbol;

  sbLibTable *table_to_use = find_method_table(target);

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
    sbLibMethod *f = sbLibTable_find_method(table_to_use, method_name_val->symbol);
    if (f) {
      if (f->is_property) {
        /* uhhh... this is theoretically possible but weird. it's like,
         * saying "list.length(string::convert)". fortunately we can just
         * sort of treat it as a normal method call again */
        f->behavior(vm, target, 0);
        if (num_params > 0) {
          sbVm_push(vm, &HVINT(num_params));
          sbLib_resolve_method(vm);
        }
      } else {
        if (num_params >= f->min_args && (num_params <= f->max_args || f->max_args == -1)) {
          f->behavior(vm, target, num_params);
        } else {
          sbVm_print_stack(vm);
          PANIC("Wrong number of arguments passed to method '%s'! (expected %d~%d, got %lld)",
              sbSymbol_name(method_name_val->symbol), f->min_args, f->max_args, (long long)num_params);
        }
      }
    } else {
      PANIC("invalid method name for type %lld: %s", (long long)target->type, sbSymbol_name(method_name_val->symbol));
    }
  }
}

/* function that resolves constructs like 'a.b' or 'a(:b)' for intrinsic types.
 * (non-intrinsic types are always functions, so they can be resolved just by
 *  calling them)*/
void sbLib_resolve_property(hVm vm) {
  hV *target = sbVm_pop(vm);
  hV *argc = sbVm_pop(vm);
  if (argc->type != IT_INTEGER) {
    CHECK("argc of send should be integer!");
  }
  if (argc->integer != 1) {
    PANIC("Illegal number of arguments!");
  }

  hV *method_name_val = sbVm_pop(vm);
  if (method_name_val->type != IT_SYMBOL) {
    PANIC("method name must be symbol! (is %zd, target %zd)", method_name_val->type, target->type);
  }

  sbLibTable *table_to_use = find_method_table(target);

  if (table_to_use == NULL) {
    /* did not find built in method table */
    if (target->type <= 0) {
      /* for built in type, just fail, it's not done yet */
      PANIC("Have not implemented this method table yet! (%lld)", (long long)target->type);
    } else {
      CHECK("This should never be reached!");
    }
  } else {
    /* built in type that has a method table defined: find method */
    sbLibMethod *f = sbLibTable_find_method(table_to_use, method_name_val->symbol);
    if (f) {
      if (f->is_property) {
        /* we found a property! great, just compute it and be done */
        f->behavior(vm, target, 0);
      } else {
        /* it is a method, but it is being called without parameters.
         * annoyingly, we need to construct a bound version of this method. */
        hRef ref = sbRef_create(target);
        sbVm_push_immediate(vm, &HVBOUNDMETHOD(method_name_val->symbol, ref));
      }
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

/* function that looks up something in a built-in method table */
static sbLibTable *find_method_table(hV *target) {
  switch(target->type) {
    case IT_LIST:
      return &g_list_methods;
    case IT_STRING:
      return &g_string_methods;
    case IT_INTEGER:
      return &g_integer_methods;
    case IT_FLOAT:
      return &g_float_methods;
    case IT_HASH:
      return &g_hash_methods;
    default:
      return NULL;
  }
}

/* function that resolves methods from BC_SEND for non intrinsic types (aka functions) */
/* we have something like a.b(c,d,e), we need to call twice to get result of a(:b)(c,d,e) */
/* resolve_method should have set us up so we can just call in twice */
static sbCFuncStatus please_call_again(hVm vm, flag init) {
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

