#include "common.h"

typedef struct sbFileReader *hFileReader;

hFileReader sbFileReader_open(const char *filename);

flag sbFileReader_ok(hFileReader r);

int sbFileReader_next(hFileReader r);

int sbFileReader_peek(hFileReader r);

int sbFileReader_get_line(hFileReader r);

int sbFileReader_get_col(hFileReader r);

void sbFileReader_fprint_line_at(FILE *out, hFileReader r, int line_num);

void sbFileReader_close(hFileReader r);
