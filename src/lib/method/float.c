#include "common.h"

#include "lib/table.h"
#include "lib/sentinel.h"
#include "data/list.h"
#include "data/string.h"
#include "vm/exec.h"

sbLibTable g_float_methods;

static void to_string(hVm vm, hVal *target, usize num_params) {
  char stackbuf[1024];
  char *buf = stackbuf;
  usize length = snprintf(buf, 1024, "%g", target->float_val);
  if (length + 1 >= sizeof(stackbuf)) {
    buf = malloc(length + 1);
    snprintf(buf, length, "%g", target->float_val);
  }
  sbVm_push_immediate(vm, &HVSTR(sbString_new(buf, length)));
  if (buf != stackbuf) {
    free(buf);
  }
}

void sbFloat_create_methods(void) {
  sbLibTable_initialize(&g_float_methods, 16, TRUE);
  REGISTER_METHOD_SYM(&g_float_methods, S_OP_TO_STRING, &PROPERTY(to_string));
}
