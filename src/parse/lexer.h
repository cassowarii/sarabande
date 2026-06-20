#include "common.h"

#include "filereader.h"
#include "token.h"
#include "scanner.h"

#define LEXER_QUEUE_LENGTH 32

typedef struct sbLexer {
    sbScanner scanner;
    sbLexToken input_queue[LEXER_QUEUE_LENGTH];
    sbLexToken output_queue[LEXER_QUEUE_LENGTH];
    int input_queue_length;
    int output_queue_length;
    sbLexToken last_token_seen;
    flag brace_terminated_state;
    sbBuffer brackets_stack;
    int n_brackets;
} sbLexer;

typedef struct sbLexer *hLexer;

void sbLexer_initialize(hLexer lx, hFileReader fr);

sbLexToken sbLexer_next(hLexer lx);

sbLexToken sbLexer_peek(hLexer lx);

void sbLexer_deinitialize(hLexer lx);
