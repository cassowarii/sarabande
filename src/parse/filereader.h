#include "common.h"

typedef struct sbFileReader *hFileReader;

hFileReader sbFileReader_open(const char *filename);

flag sbFileReader_ok(hFileReader r);

int sbFileReader_next(hFileReader r);

int sbFileReader_peek(hFileReader r);

void sbFileReader_close(hFileReader r);
