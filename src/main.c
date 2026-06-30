#include "common.h"

#include "parse/parser.h"
#include "data/string.h"
#include "data/hashtable.h"
#include "data/symbol.h"
#include "compile/ir.h"
#include "compile/emit.h"
#include "vm/exec.h"

int main(int argc, char **argv) {
  if (argc >= 2) {
    const char *filename;
    flag debugmode = FALSE;
    if (argc == 3 && sbstrncmp(argv[1], "-D", 2) == 0) {
      filename = argv[2];
      debugmode = TRUE;
    } else if (argc == 2) {
      filename = argv[1];
    } else {
      printf("usage: %s [-D] <filename>\n", argv[0]);
      printf("usage:    -D = bytecode debugger\n");
      return 0;
    }

    data_sys_init();

    sbParser pr;
    sbParser_initialize(&pr);

    sbAst parse_result = sbParser_parse_file(&pr, filename);

    if (parse_result == NULL) {
      fprintf(stderr, "fatal error: Could not open script '%s'\n", filename);
      return -2;
    } else if (parse_result->type == AST_ERROR) {
      fprintf(stderr, "fatal error: Could not run '%s' due to syntax errors.\n", filename);
      return -1;
    }

    /* syntax is valid, now try to compile */

    sbIrProgram ir = {0};

    sbIrProgram_initialize(&ir, 32768);

    sbIrProgram_compile_ast(&ir, parse_result);

    /* free AST, don't need it anymore */
    sbParser_deinitialize(&pr);

    if (ir.error_count > 0) {
      fprintf(stderr, "Could not run '%s' due to errors.\n", filename);
      return -3;
    } else {
      // print out program
      sbIrProgram_print(&ir);
    }

    sbVmProgram pm;
    sbVmProgram_initialize(&pm, 65536);

    sbEmit_compile_program(&pm, &ir);

    sbIrProgram_deinitialize(&ir);

    sbVm vm = {0};
    sbVm_initialize(&vm, 1048576, 1048576, debugmode);

    sbVm_execute(&vm, &pm);

    printf("Stack result: ");
    for (u8 *p = vm.stack; p < vm.sp; p++) {
      printf("%02X ", *p);
    }
    printf("\n");

    sbVmProgram_deinitialize(&pm);
    sbVm_deinitialize(&vm);
    data_sys_deinit();
  } else {
    fprintf(stderr, "please provide a file as input\n");
  }
}
