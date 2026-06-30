#include "common.h"

#include "parse/parser.h"
#include "data/string.h"
#include "data/hashtable.h"
#include "data/symbol.h"
#include "compile/ir.h"
#include "compile/emit.h"
#include "vm/exec.h"

int main(int argc, char **argv) {
    if (argc == 2) {
        sbString_sys_init();
        sbHash_sys_init();
        sbSymbol_sys_init();

        sbParser pr;
        sbParser_initialize(&pr);

        sbAst parse_result = sbParser_parse_file(&pr, argv[1]);

        if (parse_result == NULL) {
          fprintf(stderr, "fatal error: Could not open script '%s'\n", argv[1]);
          return -2;
        } else if (parse_result->type == AST_ERROR) {
          fprintf(stderr, "fatal error: Could not run '%s' due to syntax errors.\n", argv[1]);
          return -1;
        }

        /* syntax is valid, now try to compile */

        sbIrProgram ir = {0};

        sbIrProgram_initialize(&ir, 32768);

        sbIrProgram_compile_ast(&ir, parse_result);

        /* free AST, don't need it anymore */
        sbParser_deinitialize(&pr);

        if (ir.error_count > 0) {
          fprintf(stderr, "Could not run '%s' due to errors.\n", argv[1]);
          return -3;
        } else {
          // print out program
          sbIrProgram_print(&ir);
        }

        sbVmProgram pm;
        sbVmProgram_initialize(&pm, 65536);

        sbEmit_compile_program(&pm, &ir);

        sbIrProgram_deinitialize(&ir);

        sbVmProgram_deinitialize(&pm);
        sbSymbol_sys_deinit();
        sbHash_sys_deinit();
        sbString_sys_deinit();
    } else {
        fprintf(stderr, "please provide a file as input\n");
    }

    sbVm vm = {0};
    sbVm_initialize(&vm, 1048576, 1048576);

    sbVmProgram pm;
    sbVmProgram_initialize(&pm, 65536);

    sbVmCompiler pb = sbVmCompiler_create(4096, 4096);

    u8 code1[] = {
      BC_ALLOC_VARS, 0x02,           /* 0 1 */
      BC_LD_IMM, 0x00,    /* var1 = 0   2 3 */
      BC_ST_VAR, 0x01,    /* ...        4 5 */
      BC_LD_CONST, 0x01,  /* var0 = 9   6 7 */
      BC_ST_VAR, 0x00,    /* ...        8 9 */
      BC_LD_CONST, 0x00,  /* (LABEL) var1 += 5 ; 10 11 */
      BC_LD_VAR, 0x01,    /* ... */
      BC_OP_ADD,          /* ... */
      BC_ST_VAR, 0x01,    /* ... */
      BC_LD_VAR, 0x00,    /* var0 = var0 - 1 */
      BC_OP_DECR,         /* ... */
      BC_ST_VAR, 0x00,    /* ... */
      BC_LD_VAR, 0x00,    /* if var0 != 0 */
      BC_LD_IMM, 0x00,    /* ... */
      BC_OP_EQ,           /* ... */
      BC_JF, 0x0A,        /* ...then goto LABEL */
      BC_LD_VAR, 0x01,    /* return var1 */
      BC_LD_IMM, 0x01,    /* ... */
      BC_HALT,
    };

    sbVmCompiler_add_constant(&pb, &HVINT(5));
    sbVmCompiler_add_constant(&pb, &HVINT(9));

    sbVmCompiler_write_code(&pb, code1, sizeof(code1));
    sbVmProgram_add_block(&pm, &pb);
    sbVmCompiler_reset(&pb);

    u8 code2[] = {
      BC_LD_CONST, 0x00,
      BC_OP_ADD,
      BC_RET,
    };

    sbVmCompiler_add_constant(&pb, &HVINT(7));

    sbVmCompiler_write_code(&pb, code2, sizeof(code2));
    sbVmProgram_add_block(&pm, &pb);
    sbVmCompiler_reset(&pb);

    sbVmCompiler_deinitialize(&pb);

    sbVm_execute(&vm, &pm);

    printf("Stack result: ");
    for (u8 *p = vm.stack; p < vm.sp; p++) {
      printf("%02X ", *p);
    }
    printf("\n");
}
