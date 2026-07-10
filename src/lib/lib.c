#include "common.h"
#include "data/symbol.h"
#include "vm/exec.h"

#include "lib/table.h"
#include "lib/method.h"
#include "lib/module.h"

void sbLib_sys_init() {
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
    default:
      PANIC("Have not implemented this method table yet!");
  }

  sbLibMethod f = sbLibTable_find_method(table_to_use, method_name_val->symbol);
  if (f) {
    f(vm, target, num_params);
  } else {
    PANIC("invalid method name for type %lld: %s", (long long)target->type, sbSymbol_name(method_name_val->symbol));
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
