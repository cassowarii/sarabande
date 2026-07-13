#include "common.h"

#include "lib/table.h"
#include "lib/sentinel.h"
#include "data/list.h"
#include "data/string.h"
#include "data/symbol.h"
#include "data/integer.h"
#include "vm/exec.h"

static sbCFuncStatus from_cfunc(hVm vm, flag init);

sbLibTable g_integer_module;

static void from(hVm vm, usize argc) {
  if (argc != 1) {
    PANIC("integer::from takes 1 argument");
  }

  sbVm_call_c_func(vm, from_cfunc);
}

void sbLib_loadmodule_integer() {
  sbLibTable_initialize(&g_integer_module, 16, FALSE);
  REGISTER_VALUE(&g_integer_module, "from", &HVBUILTIN(from));
  REGISTER_VALUE(&g_integer_module, "convert", &HVSYM(S_OP_TO_INT));
}

/* --- */

static sbCFuncStatus from_cfunc(hVm vm, flag init) {
  if (!init) {
    /* get value of previous integer::convert */
    hV *value = sbVm_peek(vm, 0);
    if (value->type != IT_INTEGER) {
      PANIC("integer::convert needs to return an integer");
    }
    return CFUNC_END;
  } else {
    sbVm_push_immediate(vm, &HVSYM(S_OP_TO_INT));     /* ... target integer::convert */
    sbVm_swap(vm);                                    /* ... integer::convert target */
    sbVm_push_immediate(vm, &HVINT(1));               /* ... integer::convert target 1 */
    sbVm_swap(vm);                                    /* ... integer::convert 1 target */
    sbLib_resolve_method(vm);
    return CFUNC_NEXT;
  }
}
