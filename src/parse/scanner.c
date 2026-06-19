#include "scanner.h"

#include "filereader.h"
#include "mem/mem.h"

/* This is like, the pre lexing stage. I mean, this does a lot of the tokenizing,
 * but it includes stuff like spaces and newlines, too. Think of this as 'normalizing'
 * the input. The lexer in the second stage will use some additional context and state
 * on top of the stream of tokens output from this module in order to insert some
 * additional "ghost" tokens into the stream (invisible parentheses, semicolon insertion),
 * and will delete stuff like spaces and newlines. But we need to preserve spaces and such
 * after this stage so we can differentiate between things like 'a.b (1)' and 'a.b(1)',
 * which have different semantics. */

#define MEM_BLOCK_SIZE 65536

typedef struct sbScanner {
    hFileReader file_reader;
    hArena arena;
    sbLexToken next_token;
} sbScanner;

hScanner sbScanner_create(hFileReader fr) {
    hScanner sc = malloc(sizeof(sbScanner));
    sc->arena = sbArena_create(MEM_BLOCK_SIZE);
    sc->file_reader = fr;
    sc->next_token = (sbLexToken) {0};

    return sc;
}

sbLexToken sbScanner_peek(hScanner sc) {
    return sc->next_token;
}

void sbScanner_destroy(hScanner sc) {
    sbArena_destroy(sc->arena);
    free(sc);
}

/* yeah, yeah, unicode! get off my achin' back! i'll do it later! (maybe) */

flag is_digit(char c) {
    return ('0' <= c && c <= '9');
}

flag is_alpha(char c) {
    return ('a' <= c && c <= 'z') || ('A' <= c && c <= 'Z') || c == '_';
}

flag is_space(char c) {
    return c == ' ' || c == '\t' || c == '\r';
}

flag accept_char(char expected, hFileReader fr) {
    char actual = sbFileReader_peek(fr);
    if (expected == actual) {
        sbFileReader_next(fr);
        return 1;
    } else {
        return 0;
    }
}

char accept_digit(hFileReader fr) {
    char actual_char = sbFileReader_peek(fr);
    if (is_digit(actual_char)) {
        sbFileReader_next(fr);
        return actual_char;
    } else {
        return 0;
    }
}

char accept_alpha(hFileReader fr) {
    char actual_char = sbFileReader_peek(fr);
    if (is_alpha(actual_char)) {
        sbFileReader_next(fr);
        return actual_char;
    } else {
        return 0;
    }
}

sbLexToken compute_next_token(hScanner sc) {
    char ch = sbFileReader_peek(sc->file_reader);

    char token_buffer[256];
    int token_size = 0;

    sbLexToken new_token = {0};

    if (ch == EOF) {
        new_token.type = T_EOF;
    } else if (ch == '\n') {
        new_token.type = T_NEWLINE;

        /* skip all spaces after the newline */
        do {
            sbFileReader_next(sc->file_reader);
        } while (is_space(sbFileReader_peek(sc->file_reader)));
    } else if (is_space(ch)) {
        new_token.type = T_SPACE;

        /* skip all subsequent spaces */
        do {
            sbFileReader_next(sc->file_reader);
        } while (is_space(sbFileReader_peek(sc->file_reader)));

        /* if we run into a newline, forget about the spaces */
        if (sbFileReader_peek(sc->file_reader) == '\n') {
            new_token.type = T_NEWLINE;

            /* and skip all spaces after the newline */
            do {
                sbFileReader_next(sc->file_reader);
            } while (is_space(sbFileReader_peek(sc->file_reader)));
        }
    } else if (is_alpha(ch)) {
        new_token.type = T_IDENTIFIER;

        do {
            token_buffer[token_size] = sbFileReader_next(sc->file_reader);
            token_size ++;
            ch = sbFileReader_peek(sc->file_reader);
        } while (is_alpha(ch) || is_digit(ch));

        /* TODO: I am sleepy. Please check that this has not overflowed the buffer. */

        token_buffer[token_size] = '\0';

        char *storage = sbArena_alloc(sc->arena, token_size + 1);

        mystrncpy(storage, token_buffer, token_size + 1);

        new_token.str = storage;
    } else if (is_digit(ch)) {
        new_token.type = T_INTEGER;
        /* TODO: yes, yes. this isn't correct. listen. i am tired. */

        i64 intval = ch - '0';

        while (is_digit(sbFileReader_peek(sc->file_reader))) {
            ch = sbFileReader_next(sc->file_reader);
            intval *= 10;
            intval += ch - '0';
        }
    } else {
        new_token.type = T_ERROR;

        switch (ch) {
            case '(':
            case ')':
            case '.':
                /* TODO ok blah blah this isn't right */
                new_token.type = ch;
                break;
            default:
                fprintf(stderr, "don't know how to process character: '%c'\n", ch);
        }

        sbFileReader_next(sc->file_reader);
    }

    return new_token;
}

sbLexToken sbScanner_next(hScanner sc) {
    if (sc->next_token.type == T_NULL) {
        /* if we just started, queue up the first token */
        sc->next_token = compute_next_token(sc);
    }

    sbLexToken to_return = sc->next_token;
    sc->next_token = compute_next_token(sc);
    return to_return;
}
