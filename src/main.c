#include "common.h"

#include "parse/parser.h"
#include "data/string.h"
#include "data/hashtable.h"
#include "data/symbol.h"

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
}
