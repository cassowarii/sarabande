#include "common.h"

#include "lib/table.h"
#include "lib/sentinel.h"
#include "data/list.h"
#include "data/string.h"
#include "data/symbol.h"
#include "data/integer.h"
#include "vm/exec.h"

static sbCFuncStatus from_cfunc(hVm vm, flag init);

sbLibTable g_string_module;

static void from(hVm vm, usize argc) {
  if (argc != 1) {
    PANIC("string::from takes 1 argument");
  }

  sbVm_call_c_func(vm, from_cfunc);
}

void sbLib_loadmodule_string() {
  sbLibTable_initialize(&g_string_module, 16, FALSE);
  REGISTER_VALUE(&g_string_module, "from", &HVBUILTIN(from));
  REGISTER_VALUE(&g_string_module, "convert", &HVSYM(S_OP_TO_STRING));
}

/* --- */

static sbCFuncStatus from_cfunc(hVm vm, flag init) {
  if (!init) {
    /* get value of previous to_string */
    hV *value = sbVm_peek(vm, 0);
    /* TODO: We should probably have some kind of 'implicit convert to string'
     * thing that checks that it's really a string and throws if not, that we
     * can call from multiple places  */
    /* also this should have some kind of like default value if it doesn't work */
    if (value->type != IT_STRING) {
      PANIC("string::convert needs to return a string");
    }
    return CFUNC_END;
  } else {
    sbVm_push_immediate(vm, &HVSYM(S_OP_TO_STRING));  /* ... target string::convert */
    sbVm_swap(vm);                                    /* ... string::convert target */
    sbVm_push_immediate(vm, &HVINT(1));               /* ... string::convert target 1 */
    sbVm_swap(vm);                                    /* ... string::convert 1 target */
    sbLib_resolve_method(vm);
    return CFUNC_NEXT;
  }
}
