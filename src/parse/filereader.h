#include "common.h"

/* Phase 1 of the compiler.
 * This is exceedingly simple. We just read data from a file a line at
 * a time, and dribble out one character at a time to the next phase
 * when it asks us to. We can also report back what a certain line
 * said, if a later phase wants to highlight a syntax error, as long
 * as we haven't gone too far ahead. */

typedef struct sbFileReader *hFileReader;

hFileReader sbFileReader_open(const char *filename);

flag sbFileReader_ok(hFileReader r);

int sbFileReader_next(hFileReader r);

int sbFileReader_peek(hFileReader r);

int sbFileReader_get_line(hFileReader r);

int sbFileReader_get_col(hFileReader r);

void sbFileReader_fprint_line_at(FILE *out, hFileReader r, int line_num);

void sbFileReader_close(hFileReader r);
