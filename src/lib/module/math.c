#include "common.h"

#include "lib/table.h"
#include "data/list.h"
#include "data/string.h"
#include "data/symbol.h"
#include "vm/exec.h"

#include <math.h>

sbLibTable g_math_module;

static void sbsqrt(hVm vm, usize argc) {
  if (argc != 1) {
    PANIC("math::sqrt takes one argument");
  }
  hV *argument = sbVm_pop(vm);

  double original_value;
  if (argument->type == IT_INTEGER) {
    original_value = (double)argument->integer;
    printf("%g\n", original_value);
  } else if (argument->type == IT_FLOAT) {
    original_value = argument->float_val;
  } else {
    PANIC("can only take square root of int or float!");
  }

  sbVm_push_immediate(vm, &HVFLOAT(sqrt(original_value)));
}

void sbLib_loadmodule_math() {
  sbLibTable_initialize(&g_math_module, 16, FALSE);
  REGISTER_VALUE(&g_math_module, "sqrt", &HVBUILTIN(sbsqrt));
}
