#include "common.h"

#include "filereader.h"
#include "token.h"
#include "scanner.h"

/* Phase 3 of the compiler.
 * "Lexer" isn't a very good name for this phase. This takes
 * the stream of undifferentiated tokens coming from scanner
 * and does some basic transformations on the stream to add in
 * things like parentheses and semicolons which are implied by
 * the structure of the code but not specified. Maybe it's a
 * little too cute in places, but I kind of like it. */

typedef struct sbLexer {
    sbScanner scanner;
    sbTokenQueue input_queue;
    sbTokenQueue output_queue;
    sbBuffer brackets_stack;
    sbLexToken last_token_seen;
} sbLexer;

typedef struct sbLexer *hLexer;

void sbLexer_initialize(hLexer lx, hFileReader fr);

sbLexToken sbLexer_next(hLexer lx);

sbLexToken sbLexer_peek(hLexer lx);

void sbLexer_deinitialize(hLexer lx);
