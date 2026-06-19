#include "filereader.h"

/* this seems pointless right now as a wrapper for FILE* but later, we can make
 * this work to read from stdin in interactive mode as well, which will be easier
 * if we just have these few functions separate from the actual lexer code.
 * we may also want to have multiple files open for imports and stuff... */

struct sbFileReader {
    FILE *file;
};

hFileReader sbFileReader_open(const char *filename) {
    hFileReader fr = malloc(sizeof(struct sbFileReader));
    fr->file = fopen(filename, "r");
    if (!fr->file) {
        perror("Error opening file for reading:");
    } else {
        return fr;
    }

    return fr;
}

char sbFileReader_next(hFileReader r) {
    char c = fgetc(r->file);
    return c;
}

char sbFileReader_peek(hFileReader r) {
    char c = fgetc(r->file);
    ungetc(c, r->file);
    return c;
}

void sbFileReader_close(hFileReader r) {
    fclose(r->file);
    free(r);
}
