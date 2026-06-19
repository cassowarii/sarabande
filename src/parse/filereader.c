#include "filereader.h"

/* this seems pointless right now as a wrapper for FILE* but later, we can make
 * this work to read from stdin in interactive mode as well, which will be easier
 * if we just have these few functions separate from the actual lexer code.
 * we may also want to have multiple files open for imports and stuff... */

struct sbFileReader {
    flag valid;
    FILE *file;
};

hFileReader sbFileReader_open(const char *filename) {
    hFileReader r = malloc(sizeof(struct sbFileReader));
    r->file = fopen(filename, "r");
    if (!r->file) {
        perror("Error opening file for reading");
        r->valid = 0;
    } else {
        r->valid = 1;
    }

    return r;
}

flag sbFileReader_ok(hFileReader r) {
    return r->valid;
}

int sbFileReader_next(hFileReader r) {
    if (r->valid) {
        char c = fgetc(r->file);
        return c;
    } else {
        return EOF;
    }
}

int sbFileReader_peek(hFileReader r) {
    if (r->valid) {
        char c = fgetc(r->file);
        ungetc(c, r->file);
        return c;
    } else {
        return EOF;
    }
}

void sbFileReader_close(hFileReader r) {
    if (r->valid) {
        fclose(r->file);
    }
    free(r);
}
