#include "common.h"

#include "lib/table.h"
#include "lib/module.h"
#include "lib/sentinel.h"
#include "data/list.h"
#include "data/string.h"
#include "data/symbol.h"
#include "vm/exec.h"

sbCFuncStatus print_cfunc(hVm vm, flag init);
sbCFuncStatus println_cfunc(hVm vm, flag init);

sbLibTable g_global_module;

sbLibTable g_string_namespace;

extern sbLibTable g_math_module;

static void print(hVm vm, usize argc) {
  sbVm_push_immediate(vm, &HVINT(argc));
  sbVm_call_c_func(vm, print_cfunc);
}

static void println(hVm vm, usize argc) {
  sbVm_push_immediate(vm, &HVINT(argc));
  sbVm_call_c_func(vm, println_cfunc);
}

void sbLib_loadmodule_global() {
  sbLibTable_initialize(&g_global_module, 16, FALSE);
  REGISTER_VALUE(&g_global_module, "print", &HVBUILTIN(print));
  REGISTER_VALUE(&g_global_module, "println", &HVBUILTIN(println));

  sbLibTable_initialize(&g_string_namespace, 16, FALSE);
  REGISTER_VALUE(&g_string_namespace, "convert", &HVSYM(S_OP_TO_STRING));
  REGISTER_VALUE(&g_global_module, "string", &HVMODULE(&g_string_namespace));

  sbLib_loadmodule_list();
  REGISTER_VALUE(&g_global_module, "list", &HVMODULE(&g_list_module));

  sbLib_loadmodule_math();
  REGISTER_VALUE(&g_global_module, "math", &HVMODULE(&g_math_module));
}

/* --- */

sbCFuncStatus print_cfunc(hVm vm, flag init) {
  hV *args_left;

  if (!init) {
    /* get value of previous to_string */
    hV *value = sbVm_pop(vm);
    /* TODO: We should probably have some kind of 'implicit convert to string'
     * thing that checks that it's really a string and throws if not, that we
     * can call from multiple places  */
    if (value->type != IT_STRING) {
      PANIC("string::convert needs to return a string");
    }

    char scratch[8];
    const char *strdata = sbString_get_value(value->string, scratch, NULL);
    printf("%s", strdata);
    args_left = sbVm_pop(vm);
  } else {
    args_left = sbVm_pop(vm);
    if (args_left->type != IT_INTEGER) {
      CHECK("argc to function needs to be integer!");
    }
  }

  if (args_left->integer > 0) {
    args_left->integer --;
    sbVm_push_immediate(vm, args_left);               /* ... param args_left */
    sbVm_swap(vm);                                    /* ... args_left param */
    sbVm_push_immediate(vm, &HVSYM(S_OP_TO_STRING));  /* ... args_left param string::convert*/
    sbVm_swap(vm);                                    /* ... args_left string::convert param */
    sbVm_push_immediate(vm, &HVINT(1));               /* ... args_left string::convert param 1 */
    sbVm_swap(vm);                                    /* ... args_left string::convert 1 param */
    sbLib_resolve_method(vm);                         /* call method */
    return CFUNC_NEXT;
  } else {
    sbVm_push_immediate(vm, &HVNIL);
    return CFUNC_END;
  }
}

sbCFuncStatus println_cfunc(hVm vm, flag init) {
  if (init) {
    sbVm_call_c_func(vm, print_cfunc);
    return CFUNC_NEXT;
  } else {
    printf("\n");
    return CFUNC_END;
  }
}
