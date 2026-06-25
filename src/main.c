#include "common.h"

#include "parse/filereader.h"
#include "parse/lexer.h"
#include "data/string.h"
#include "data/hashtable.h"
#include "data/symbol.h"

int main(int argc, char **argv) {
    if (argc == 2) {
        hFileReader fr = sbFileReader_open(argv[1]);

        if (!sbFileReader_ok(fr)) {
            fprintf(stderr, "Error getting input stream.\n");
            return -1;
        }

        sbLexer lx;
        sbLexer_initialize(&lx, fr);

        sbString_sys_init();
        sbHash_sys_init();
        sbSymbol_sys_init();

        sbLexToken t;
        char scratch[8];

        printf("ok here i go\n");
        hString hello_str = OBJSL("hello! ");
        hString long_str = OBJSL("");
        hString test_str = OBJSL("hello! hello! hello! hello! hello! ");
        for (int i = 0; i < 300; i++) {
          sbString_release(long_str);
          long_str = sbString_concat(long_str, hello_str);
          if (sbString_eq(test_str, long_str)) {
            printf("BREAK!\n");
            break;
          }
        }
        printf("%s\n", sbString_get_value(long_str, scratch, NULL));

        do {
            t = sbLexer_next(&lx);

            if (t.type == T_EOF) {
                printf("<EOF>");
            } else if (t.type == T_SPACE) {
                printf("<SPACE>");
            } else if (t.type == T_NEWLINE) {
                printf("\n");
            } else if (t.type == T_IDENTIFIER) {
                printf("%s", t.cstr);
            } else if (t.type == T_INTEGER) {
                printf("<INT %d>", t.i);
            } else if (t.type == T_STRING) {
                printf("<STRING '%s'>", sbString_get_value(t.hstr, scratch, NULL));
            } else if (t.type == T_SYMBOL) {
                printf("<SYMBOL :%s>", t.cstr);
            } else if (t.type >= T_rAND && t.type <= T_rWHILE) {
                printf("<%s>", t.cstr);
            } else if (t.type == T_SEMICOLON) {
                printf(";\n");
            } else {
                switch (t.type) {
                    case T_COLONBRACE:
                        printf(":{"); break;
                    case T_DOUBLEEQUALS:
                        printf("=="); break;
                    default:
                        printf("%c", t.type);
                }
            }
        } while (t.type != T_EOF);
        printf("\n");

        sbLexer_deinitialize(&lx);

        sbFileReader_close(fr);

        sbString_sys_deinit();
    } else {
        fprintf(stderr, "please provide a file as input\n");
    }
}
