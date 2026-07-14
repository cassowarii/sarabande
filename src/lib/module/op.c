#include "common.h"

#include "lib/table.h"
#include "lib/sentinel.h"
#include "data/list.h"
#include "data/symbol.h"
#include "vm/exec.h"

sbLibTable g_op_module;

void sbLib_loadmodule_op() {
  sbLibTable_initialize(&g_op_module, 16, FALSE);
  REGISTER_VALUE(&g_op_module, "add", &HVSYM(S_OP_ADD));
  REGISTER_VALUE(&g_op_module, "sub", &HVSYM(S_OP_SUB));
  REGISTER_VALUE(&g_op_module, "mul", &HVSYM(S_OP_MUL));
  REGISTER_VALUE(&g_op_module, "div", &HVSYM(S_OP_DIV));
  REGISTER_VALUE(&g_op_module, "intdiv", &HVSYM(S_OP_INTDIV));
  REGISTER_VALUE(&g_op_module, "mod", &HVSYM(S_OP_MOD));
  REGISTER_VALUE(&g_op_module, "eq", &HVSYM(S_OP_EQ));
  REGISTER_VALUE(&g_op_module, "lt", &HVSYM(S_OP_LT));
  REGISTER_VALUE(&g_op_module, "le", &HVSYM(S_OP_LE));
  REGISTER_VALUE(&g_op_module, "call", &HVSYM(S_OP_CALL));
  REGISTER_VALUE(&g_op_module, "set", &HVSYM(S_OP_SET));
  REGISTER_VALUE(&g_op_module, "index", &HVSYM(S_OP_INDEX));
  REGISTER_VALUE(&g_op_module, "setindex", &HVSYM(S_OP_SETINDEX));
  REGISTER_VALUE(&g_op_module, "rangeindex", &HVSYM(S_OP_RANGEINDEX));
  REGISTER_VALUE(&g_op_module, "setrangeindex", &HVSYM(S_OP_SETRANGEINDEX));
}
