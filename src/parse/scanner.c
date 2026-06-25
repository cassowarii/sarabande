#include "scanner.h"

#include "data/string.h"
#include "data/symbol.h"

/* This is like, the pre lexing stage. I mean, this does a lot of the tokenizing,
 * but it includes stuff like spaces and newlines, too. Think of this as 'normalizing'
 * the input. The lexer in the second stage will use some additional context and state
 * on top of the stream of tokens output from this module in order to insert some
 * additional "ghost" tokens into the stream (invisible parentheses, semicolon insertion),
 * and will delete stuff like spaces and newlines. But we need to preserve spaces and such
 * after this stage so we can differentiate between things like 'a.b (1)' and 'a.b(1)',
 * which have different semantics. */

#define MEM_BLOCK_SIZE 65536
#define TOKEN_BUFFER_SIZE 64

struct ReservedWord {
    const char *name;
    sbTokenType token_type;
};

static struct ReservedWord reserved_words[] = {
    { "and", T_rAND },
    { "as", T_rAS },
    { "case", T_rCASE },
    { "def", T_rDEF },
    { "do", T_rDO },
    { "else", T_rELSE },
    { "false", T_rFALSE },
    { "if", T_rIF },
    { "let", T_rLET },
    { "in", T_rIN },
    { "match", T_rMATCH },
    { "not", T_rNOT },
    { "or", T_rOR },
    { "repeat", T_rREPEAT },
    { "return", T_rRETURN },
    { "true", T_rTRUE},
    { "unless", T_rUNLESS },
    { "until", T_rUNTIL },
    { "when", T_rWHEN },
    { "while", T_rWHILE },
};

#define N_RESERVED_WORDS ((sizeof(reserved_words))/(sizeof(reserved_words[0])))

static void check_if_start(hScanner sc);
static sbLexToken compute_next_token(hScanner sc);

void sbScanner_initialize(hScanner sc, hFileReader fr) {
    sbArena_initialize(&sc->arena, MEM_BLOCK_SIZE);
    sc->file_reader = fr;
    sc->next_token = (sbLexToken) {0};

    sbBuffer_initialize(&sc->dynamic_buffer, 8);
}

sbLexToken sbScanner_peek(hScanner sc) {
    check_if_start(sc);
    return sc->next_token;
}

sbLexToken sbScanner_next(hScanner sc) {
    check_if_start(sc);
    sbLexToken to_return = sc->next_token;
    sc->next_token = compute_next_token(sc);
    return to_return;
}

void sbScanner_deinitialize(hScanner sc) {
    sbArena_destroy(&sc->arena);
    sbBuffer_deinitialize(&sc->dynamic_buffer);
}

/* yeah, yeah, unicode! get off my achin' back! i'll do it later! (maybe) */

static flag is_digit(char c) {
    return ('0' <= c && c <= '9');
}

static flag is_alpha(char c) {
    return ('a' <= c && c <= 'z') || ('A' <= c && c <= 'Z') || c == '_';
}

static flag is_space(char c) {
    return c == ' ' || c == '\t' || c == '\r';
}

static flag is_start_identifier(char c) {
    return is_alpha(c);
}

static flag is_mid_identifier(char c) {
    return is_start_identifier(c) || is_digit(c);
}

static flag is_end_identifier(char c) {
    return is_mid_identifier(c) || c == '?';
}

static flag is_start_symbol(char c) {
    return is_start_identifier(c);
}

static flag is_mid_symbol(char c) {
    return is_mid_identifier(c);
}

static flag is_end_symbol(char c) {
    return is_end_identifier(c) || c == '=';
}

static flag is_base_digit(char c, int base) {
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

static int base_digit_value(char c) {
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

char token_buffer[TOKEN_BUFFER_SIZE];
usize tb_length = 0;

static void finalize_char_buffer(hScanner sc) {
    /* move the currently buffered string into the dynamic_buffer */
    sbBuffer_append(&sc->dynamic_buffer, token_buffer, tb_length);
    tb_length = 0;
}

static void read_char_into_buffer(hScanner sc, char c) {
    token_buffer[tb_length++] = c;
    if (tb_length == TOKEN_BUFFER_SIZE) {
        finalize_char_buffer(sc);
    }
}

static usize read_identifier(hScanner sc) {
    usize token_size = 0;
    char ch = PEEK;

    do {
        read_char_into_buffer(sc, ch);
        token_size ++;
        ch = NEXT;
    } while (is_mid_identifier(ch));

    if (is_end_identifier(ch)) {
        read_char_into_buffer(sc, ch);
        NEXT;
        token_size ++;
    }

    read_char_into_buffer(sc, '\0');
    finalize_char_buffer(sc);

    return token_size;
}

static usize read_symbol(hScanner sc) {
    usize token_size = 0;
    char ch = PEEK;

    do {
        read_char_into_buffer(sc, ch);
        token_size ++;
        ch = NEXT;
    } while (is_mid_symbol(ch));

    if (is_end_symbol(ch)) {
        read_char_into_buffer(sc, ch);
        NEXT;
        token_size ++;
    }

    read_char_into_buffer(sc, '\0');
    finalize_char_buffer(sc);

    return token_size;
}

static sbLexToken compute_next_token(hScanner sc) {
    int ch_int = PEEK;
    unsigned char ch = (unsigned char)ch_int;

    sbBuffer_reset(&sc->dynamic_buffer);

    usize token_size = 0;
    sbLexToken new_token = {0};

    if (ch_int == EOF) {
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
            ch = NEXT;
        } while (ch != '\n');

        /* eat newline, and skip all spaces after the newline */
        do {
            NEXT;
        } while (is_space(PEEK));

        new_token.type = T_NEWLINE;
    } else if (ch == '(' || ch == ')'
            || ch == '[' || ch == ']'
            || ch == '{' || ch == '}'
            || ch == ';' || ch == '|'
            || ch == ',' || ch == '\\') {
        /* unambiguously single-character tokens */
        new_token.type = ch;
        NEXT;
    } else if (ch == '!') {
        ch = NEXT;
        if (ch == '=') {
            new_token.type = T_NOTEQUALS;
            NEXT;
        } else {
            /* ! on its own is not an operator i guess. not sure how to handle this elegantly */
            fprintf(stderr, "don't know how to handle character: '!'\n");
            new_token.type = T_ERROR;
        }
    } else if (ch == '~') {
        ch = NEXT;
        if (ch == '>') {
            new_token.type = T_SQUIGARROW;
            NEXT;
        } else {
            /* ~ on its own is not an operator */
            fprintf(stderr, "don't know how to handle character: '~'\n");
            new_token.type = T_ERROR;
        }
    } else if (ch == '>') {
        ch = NEXT;
        if (ch == '>') {
            new_token.type = T_DOUBLEGREATER;
            NEXT;
        } else if (ch == '=') {
            new_token.type = T_GREATEREQUALS;
            NEXT;
        } else {
            new_token.type = T_GREATER;
        }
    } else if (ch == '.') {
        new_token.type = T_DOT;
        ch = NEXT;

        if (ch == '.') {
            new_token.type = T_TWODOT;
            ch = NEXT;

            if (ch == '.') {
                new_token.type = T_ELLIPSIS;
                NEXT;
            }
        }
    } else if (ch == '<') {
        ch = NEXT;
        if (ch == '<') {
            new_token.type = T_DOUBLEGREATER;
            NEXT;
        } else if (ch == '=') {
            new_token.type = T_LESSEQUALS;
            NEXT;
        } else if (ch == '~') {
            new_token.type = T_BACKSQUIGARROW;
            NEXT;
        } else {
            new_token.type = T_LESS;
        }
    } else if (ch == '%') {
        ch = NEXT;
        if (ch == '=') {
            new_token.type = T_PERCENTEQUALS;
            NEXT;
        } else {
            new_token.type = T_PERCENT;
        }
    } else if (ch == '/') {
        ch = NEXT;
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
        ch = NEXT;
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
        ch = NEXT;
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
        ch = NEXT;
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
        ch = NEXT;
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
        ch = NEXT;
        if (ch == ':') {
            /* :: */
            new_token.type = T_PAAMAYIM_NEKUDOTAYIM;
            NEXT;
        } else if (ch == '{') {
            /* :{ */
            new_token.type = T_COLONBRACE;
            NEXT;
        } else if (is_start_symbol(ch)) {
            /* :abc... */
            new_token.type = T_SYMBOL;

            token_size = read_symbol(sc);
            new_token.symb = sbSymbol_from_bytes(sc->dynamic_buffer.data, sc->dynamic_buffer.size - 1);
            printf("%s -> %p\n", (char*)new_token.symb, (void*)new_token.symb);
            new_token.size = token_size;
        } else {
            /* : */
            new_token.type = T_COLON;
        }
    } else if (is_space(ch)) {
        new_token.type = T_SPACE;

        /* skip all subsequent spaces */
        do {
            ch = NEXT;
        } while (is_space(ch));

        /* if we run into a comment, skip to the end of the line */
        if (ch == '#') {
            do {
                ch = NEXT;
            } while (ch != '\n');
        }

        /* if we run into a newline, forget about the spaces */
        if (ch == '\n') {
            new_token.type = T_NEWLINE;

            /* and skip all spaces after the newline */
            do {
                ch = NEXT;
            } while (is_space(ch));
        }
    } else if (ch == '`') {
        /* Raw string */
        new_token.type = T_STRING;
        ch = NEXT;

        flag in_string = 1;

        while (in_string) {
            while (ch != '`') {
                read_char_into_buffer(sc, ch);
                ch = NEXT;
            }

            /* ok, now we know ch is a backtick. discard it and look at what's next */
            ch = NEXT;

            if (ch == '`') {
                /* it was a double backtick. so count this as one backtick (escaped) and
                 * continue reading into the string */
                read_char_into_buffer(sc, ch);
                ch = NEXT;
            } else {
                /* it is something else. end of string. exit loop. */
                in_string = 0;
            }
        }

        read_char_into_buffer(sc, '\0');

        finalize_char_buffer(sc);
        hString hstr = sbString_new(sc->dynamic_buffer.data, sc->dynamic_buffer.size - 1);

        new_token.hstr = hstr;
        new_token.size = sc->dynamic_buffer.size;
    } else if (is_start_identifier(ch)) {
        new_token.type = T_IDENTIFIER;

        token_size = read_identifier(sc);
        new_token.symb = sbSymbol_from_bytes(sc->dynamic_buffer.data, sc->dynamic_buffer.size - 1);
        printf("%s -> %p\n", (char*)new_token.symb, (void*)new_token.symb);
        new_token.size = token_size;
    } else if (is_digit(ch)) {
        new_token.type = T_INTEGER;

        i64 intval = 0;
        int base = 10;

        if (ch == '0') {
            /* skip a leading zero, and see if it has some base indicator */
            ch = NEXT;
            if (ch == 'b') {
                ch = NEXT;
                base = 2;
            } else if (ch == 'o') {
                ch = NEXT;
                base = 8;
            } else if (ch == 'x') {
                ch = NEXT;
                base = 16;
            }
        }

        do {
            /* TODO: promote to bigint, don't allow overflow */
            intval *= base;
            intval += base_digit_value(ch);
            do {
                /* skip over underscores in numeric literals */
                ch = NEXT;
            } while (ch == '_');
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

    /* if we receive an identifier, check if it is a reserved word or not. */
    if (new_token.type == T_IDENTIFIER) {
        for (int i = 0; i < N_RESERVED_WORDS; i++) {
            if (!sbstrncmp(reserved_words[i].name, new_token.cstr, new_token.size + 1)) {
                new_token.type = reserved_words[i].token_type;
                break;
            }
        }
    }


    return new_token;
}

static void check_if_start(hScanner sc) {
    if (sc->next_token.type == T_NULL) {
        /* if we just started, queue up the first token */
        sc->next_token = compute_next_token(sc);
    }
}
