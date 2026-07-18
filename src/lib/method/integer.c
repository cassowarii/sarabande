#include "common.h"

#include "lib/table.h"
#include "lib/sentinel.h"
#include "data/list.h"
#include "data/string.h"
#include "data/integer.h"
#include "vm/exec.h"

sbLibTable g_integer_methods;

static void to_string(hVm vm, hVal *target, usize num_params) {
  char stackbuf[1024];
  char *buf = stackbuf;
  usize length = sbInteger_snprint(buf, 1024, target->integer);
  if (length + 1 >= sizeof(stackbuf)) {
    buf = malloc(length + 1);
    sbInteger_snprint(buf, length, target->integer);
  }
  sbVm_push_immediate(vm, &HVSTR(sbString_new(buf, length)));
  if (buf != stackbuf) {
    free(buf);
  }
}

void sbInteger_create_methods(void) {
  sbLibTable_initialize(&g_integer_methods, 16, TRUE);
  REGISTER_METHOD_SYM(&g_integer_methods, S_OP_TO_STRING, &PROPERTY(to_string));
}
