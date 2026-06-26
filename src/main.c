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

        sbParser_parse_file(&pr, argv[1]);

        sbParser_deinitialize(&pr);

        sbSymbol_sys_deinit();
        sbHash_sys_deinit();
        sbString_sys_deinit();
    } else {
        fprintf(stderr, "please provide a file as input\n");
    }
}
