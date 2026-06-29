#ifndef __SARABANDE_SCANNER_H__
#define __SARABANDE_SCANNER_H__

#include "common.h"

#include "filereader.h"
#include "token.h"
#include "mem/mem.h"

/* Phase 2 of the compiler.
 * This takes the stream of single characters coming from filereader
 * and groups them together into individual tokens which have a type.
 * We recognize things like operators, identifiers, numbers, and
 * strings here. */

typedef struct sbScanner {
    sbLexToken next_token;
    hFileReader file_reader;
    sbBuffer dynamic_buffer;
} sbScanner;

typedef sbScanner *hScanner;

void sbScanner_initialize(hScanner sc, hFileReader fr);

sbLexToken sbScanner_next(hScanner sc);

sbLexToken sbScanner_peek(hScanner sc);

void sbScanner_deinitialize(hScanner sc);

#endif
