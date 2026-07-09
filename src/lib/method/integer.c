#include "common.h"

#include "lib/table.h"
#include "data/list.h"
#include "data/string.h"
#include "vm/exec.h"

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
  REGISTER_METHOD(&g_integer_methods, "to_string", to_string);
}
