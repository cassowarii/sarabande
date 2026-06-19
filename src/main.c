#include "common.h"

#include "parse/filereader.h"
#include "parse/scanner.h"

int main(int argc, char **argv) {
    if (argc == 2) {
        hFileReader fr = sbFileReader_open(argv[1]);

        hScanner sc = sbScanner_create(fr);

        sbLexToken t;

        do {
            t = sbScanner_next(sc);

            if (t.type == T_EOF) {
                printf("(EOF)");
            } else if (t.type == T_SPACE) {
                printf("( )");
            } else if (t.type == T_NEWLINE) {
                printf("(\\n)");
            } else if (t.type == T_IDENTIFIER) {
                printf("(ID %s)", t.str);
            } else if (t.type == T_INTEGER) {
                printf("(INT %d)", t.i);
            } else {
                printf("  %c  ", t.type);
            }
        } while (t.type != T_EOF);

        sbScanner_destroy(sc);

        sbFileReader_close(fr);
    } else {
        fprintf(stderr, "please provide a file as input\n");
    }
}
