#include "common.h"

#include "filereader.h"
#include "token.h"
#include "scanner.h"

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
