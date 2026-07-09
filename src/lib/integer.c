#include "lib/libtable.h"

#include "data/list.h"
#include "data/string.h"
#include "vm/exec.h"

void list_each_cfunc(hVm vm, flag init);
void list_map_cfunc(hVm vm, flag init);
void list_filter_cfunc(hVm vm, flag init);
void list_any_cfunc(hVm vm, flag init);
void list_all_cfunc(hVm vm, flag init);

sbLibTable g_integer_methods;

static void to_string(hVm vm, hV *target, usize num_params) {
  sbVm_pop(vm); /* remove method name */
  char stackbuf[1024];
  char *buf = stackbuf;
  usize length = snprintf(buf, 1024, "%lld", (long long)target->integer);
  if (length + 1 >= sizeof(stackbuf)) {
    buf = malloc(length + 1);
    snprintf(buf, length, "%lld", (long long)target->integer);
  }
  sbVm_push_immediate(vm, &HVSTR(sbString_new(buf, length)));
  if (buf != stackbuf) {
    free(buf);
  }
}

void sbInteger_create_methods(void) {
  LIB_REGISTER(&g_integer_methods, "to_string", to_string);
}
