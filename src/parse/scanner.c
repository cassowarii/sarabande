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
#define TOKEN_BUFFER_SIZE 256

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

flag is_start_identifier(char c) {
    return is_alpha(c);
}

flag is_mid_identifier(char c) {
    return is_start_identifier(c) || is_digit(c);
}

flag is_end_identifier(char c) {
    return is_mid_identifier(c) || c == '?';
}

flag is_base_digit(char c, int base) {
    if (base == 10) {
        return is_digit(c);
    } else if (base == 16) {
        return is_digit(c) || ('a' <= c && c <= 'f') || ('A' <= c && c <= 'F');
    } else if (base == 8) {
        return '0' <= c && c <= '7';
    } else if (base == 2) {
        return '0' <= c && c <= '1';
    } else {
        return 0;
    }
}

int base_digit_value(char c) {
    if (is_digit(c)) {
        return c - '0';
    } else if ('a' <= c && c <= 'z') {
        return c - 'a' + 10;
    } else if ('A' <= c && c <= 'Z') {
        return c - 'A' + 10;
    } else {
        return 0;
    }
}

#define NEXT sbFileReader_next(sc->file_reader)
#define PEEK sbFileReader_peek(sc->file_reader)

usize read_identifier(hScanner sc, char *token_buffer, usize max_size) {
    usize token_size = 0;
    char ch = 0;

    do {
        token_buffer[token_size] = NEXT;
        token_size ++;
        ch = PEEK;
    } while (is_mid_identifier(ch) && token_size < max_size - 1);

    if (is_end_identifier(ch) && token_size < max_size - 1) {
        token_buffer[token_size] = NEXT;
        token_size ++;
    }

    token_buffer[token_size] = '\0';

    return token_size;
}

sbLexToken compute_next_token(hScanner sc) {
    int next = PEEK;
    char ch = (char)next;

    char token_buffer[TOKEN_BUFFER_SIZE];
    usize token_size;

    sbLexToken new_token = {0};

    if (next == EOF) {
        new_token.type = T_EOF;
    } else if (ch == '\n') {
        new_token.type = T_NEWLINE;

        /* skip all spaces after the newline */
        do {
            NEXT;
        } while (is_space(PEEK));
    } else if (ch == '#') {
        /* Comment. Ignore everything until the end of the line. */
        do {
            NEXT;
            ch = PEEK;
        } while (ch != '\n');
        NEXT; /* eat the newline */
        new_token.type = T_NEWLINE;
    } else if (ch == '(' || ch == ')'
            || ch == '[' || ch == ']'
            || ch == '{' || ch == '}'
            || ch == ';' || ch == '|'
            || ch == '.' || ch == ','
            || ch == '\\') {
        /* unambiguously single-character tokens */
        new_token.type = ch;
        NEXT;
    } else if (ch == '%') {
        NEXT;
        ch = PEEK;
        if (ch == '=') {
            new_token.type = T_PERCENTEQUALS;
        } else {
            new_token.type = T_PERCENT;
        }
    } else if (ch == '/') {
        NEXT;
        ch = PEEK;
        if (ch == '/') {
            new_token.type = T_DOUBLESLASH;
            NEXT;
        } else if (ch == '=') {
            new_token.type = T_SLASHEQUALS;
            NEXT;
        } else {
            new_token.type = T_SLASH;
        }
    } else if (ch == '*') {
        NEXT;
        ch = PEEK;
        if (ch == '*') {
            new_token.type = T_DOUBLEASTERISK;
            NEXT;
        } else if (ch == '=') {
            new_token.type = T_ASTERISKEQUALS;
            NEXT;
        } else {
            new_token.type = T_ASTERISK;
        }
    } else if (ch == '=') {
        NEXT;
        ch = PEEK;
        if (ch == '=') {
            new_token.type = T_DOUBLEEQUALS;
            NEXT;
        } else if (ch == '>') {
            new_token.type = T_FATARROW;
            NEXT;
        } else {
            new_token.type = T_EQUALS;
        }
    } else if (ch == '+') {
        NEXT;
        ch = PEEK;
        if (ch == '+') {
            new_token.type = T_DOUBLEPLUS;
            NEXT;
        } else if (ch == '=') {
            new_token.type = T_PLUSEQUALS;
            NEXT;
        } else {
            new_token.type = T_PLUS;
        }
    } else if (ch == '-') {
        NEXT;
        ch = PEEK;
        if (ch == '-') {
            new_token.type = T_DOUBLEMINUS;
            NEXT;
        } else if (ch == '>') {
            new_token.type = T_ARROW;
            NEXT;
        } else if (ch == '=') {
            new_token.type = T_MINUSEQUALS;
            NEXT;
        } else {
            new_token.type = T_MINUS;
        }
    } else if (ch == ':') {
        NEXT;
        ch = PEEK;
        if (ch == ':') {
            /* :: */
            new_token.type = T_PAAMAYIM_NEKUDOTAYIM;
        } else if (ch == '{') {
            /* :{ */
            new_token.type = T_COLONBRACE;
        } else if (is_start_identifier(ch)) {
            /* :abc... */
            new_token.type = T_SYMBOL;

            token_size = read_identifier(sc, token_buffer, TOKEN_BUFFER_SIZE);

            char *storage = sbArena_alloc(sc->arena, token_size + 1);

            sbstrncpy(storage, token_buffer, token_size + 1);

            new_token.str = storage;
            new_token.size = token_size;
        } else {
            /* : */
            new_token.type = T_COLON;
        }
    } else if (is_space(ch)) {
        new_token.type = T_SPACE;

        /* skip all subsequent spaces */
        do {
            NEXT;
        } while (is_space(PEEK));

        /* if we run into a comment, skip to the end of the line */
        if (ch == '#') {
            do {
                NEXT;
            } while (PEEK != '\n');
        }

        /* if we run into a newline, forget about the spaces */
        if (PEEK == '\n') {
            new_token.type = T_NEWLINE;

            /* and skip all spaces after the newline */
            do {
                NEXT;
            } while (is_space(PEEK));
        }
    } else if (is_start_identifier(ch)) {
        new_token.type = T_IDENTIFIER;

        token_size = read_identifier(sc, token_buffer, TOKEN_BUFFER_SIZE);

        char *storage = sbArena_alloc(sc->arena, token_size + 1);

        sbstrncpy(storage, token_buffer, token_size + 1);

        new_token.str = storage;
        new_token.size = token_size;
    } else if (is_digit(ch)) {
        new_token.type = T_INTEGER;

        i64 intval = 0;
        int base = 10;

        if (ch == '0') {
            NEXT;
            ch = PEEK;
            if (ch == 'b') {
                NEXT;
                ch = PEEK;
                base = 2;
            } else if (ch == 'o') {
                NEXT;
                ch = PEEK;
                base = 8;
            } else if (ch == 'x') {
                NEXT;
                ch = PEEK;
                base = 16;
            }
        }

        do {
            /* TODO: promote to bigint, don't allow overflow */
            NEXT;
            intval *= base;
            intval += base_digit_value(ch);
            ch = PEEK;
        } while (is_base_digit(ch, base));

        /* if we are now looking at still a character that's a letter or digit, throw error */
        if (is_digit(ch) || is_alpha(ch)) {
            if (base == 10) {
                fprintf(stderr, "unexpected character in numeric literal: '%c'\n", ch);
            } else {
                fprintf(stderr, "unexpected character in base %d numeric literal: '%c'\n", base, ch);
            }
            new_token.type = T_ERROR;
            NEXT;
        } else {
            new_token.i = intval;
        }
    } else {
        new_token.type = T_ERROR;

        fprintf(stderr, "don't know how to process character: '%c'\n", ch);

        NEXT;
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
