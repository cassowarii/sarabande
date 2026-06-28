#include "common.h"

#include "parse/parser.h"
#include "data/string.h"
#include "data/hashtable.h"
#include "data/symbol.h"
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
        } else if (parse_result->type == AST_ERROR) {
          fprintf(stderr, "fatal error: Could not run '%s' due to syntax errors.\n", argv[1]);
        } else {
          printf("wow! the syntax is good!\n");
        }

        sbParser_deinitialize(&pr);

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

    sbVmPartialBlock pb = sbVmBlock_create(4096, 4096);

    u8 code[] = {
      BC_LD_CONST, 0x00,
      BC_LD_CONST, 0x01,
      BC_OP_ADD,
      BC_HALT,
    };

    sbVmBlock_write_code(&pb, code, sizeof(code));

    sbVmBlock_add_constant(&pb, &HVINT(5));
    sbVmBlock_add_constant(&pb, &HVINT(11));

    sbVmProgram_add_block(&pm, &pb);

    sbVm_execute(&vm, &pm);

    printf("Stack result: ");
    for (u8 *p = vm.stack; p < vm.sp; p++) {
      printf("%02X ", *p);
    }
    printf("\n");

    sbVmBlock_deinitialize(&pb);
}
