#include "common.h"

#include "lib/table.h"
#include "data/list.h"
#include "data/string.h"
#include "vm/exec.h"

void list_each_cfunc(hVm vm, flag init);
void list_map_cfunc(hVm vm, flag init);
void list_filter_cfunc(hVm vm, flag init);
void list_any_cfunc(hVm vm, flag init);
void list_all_cfunc(hVm vm, flag init);

sbLibTable g_string_methods;

static void split(hVm vm, hV *target, usize num_params) {
  sbVm_pop(vm); /* remove method name */
  usize length;
  char scratch[8];
  const char *buf = sbString_get_value(target->string, scratch, &length);
  hList l = sbList_new(length);
  for (usize i = 0; i < length; i++) {
    sbList_append(l, &HVSTR(sbString_new(&buf[i], 1)));
  }
  sbVm_push_immediate(vm, &HVLIST(l));
}

static void to_string(hVm vm, hV *target, usize num_params) {
  if (num_params != 0) {
    PANIC("to_string takes no parameters");
  }
  sbVm_pop(vm); /* remove method name */

  /* to_string for a string just returns itself */
  sbVm_push_immediate(vm, target);
}

void sbString_create_methods(void) {
  REGISTER_METHOD(&g_string_methods, "split", split);
  REGISTER_METHOD(&g_string_methods, "to_string", to_string);
}
