#include "common.h"

typedef struct sbFileReader *hFileReader;

hFileReader sbFileReader_open(const char *filename);

char sbFileReader_next(hFileReader r);

char sbFileReader_peek(hFileReader r);

void sbFileReader_close(hFileReader r);
