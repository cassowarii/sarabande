#include "common.h"
#include "lib/table.h"
#include "data/symbol.h"
#include "vm/exec.h"

extern void sbList_create_methods();
extern void sbString_create_methods();
extern void sbInteger_create_methods();

void sbLib_sys_init() {
  sbLibTable_initialize(&g_list_methods, 16, TRUE);
  sbLibTable_initialize(&g_string_methods, 16, TRUE);
  sbLibTable_initialize(&g_integer_methods, 16, TRUE);

  sbList_create_methods();
  sbString_create_methods();
  sbInteger_create_methods();
}

void sbLib_sys_deinit() {
  sbLibTable_deinitialize(&g_list_methods);
  sbLibTable_deinitialize(&g_string_methods);
  sbLibTable_deinitialize(&g_integer_methods);
}

void sbLib_resolve_method(hVm vm) {
  hV *target = sbVm_pop(vm);
  hV *argc = sbVm_pop(vm);
  if (argc->type != IT_INTEGER) {
    CHECK("argc of send should be integer!");
  }
  /* subtract 1 because the method name is itself a param */
  usize num_params = argc->integer - 1;
  hV *method_name_val = sbVm_peek(vm, num_params);
  if (method_name_val->type != IT_SYMBOL) {
    /* TODO this may become not true */
    PANIC("method name must be symbol!");
  }

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
