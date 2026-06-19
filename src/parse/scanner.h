#include "common.h"

#include "filereader.h"

typedef struct sbScanner *hScanner;

typedef enum sbTokenType {
    T_LPAREN = '(',
    T_RPAREN = ')',
    T_LBRACKET = '[',
    T_RBRACKET = '[',
    T_LBRACE = '{',
    T_RBRACE = '}',
    T_ASTERISK = '*',
    T_SLASH = '*',
    T_PLUS = '+',
    T_MINUS = '-',
    T_PERCENT = '%',
    T_PIPE = '|',
    T_DOT = '.',
    T_COMMA = ',',
    T_COLON = ':',
    T_SEMICOLON = ';',
    T_NEWLINE = '\n',
    T_SPACE = ' ',
    T_NULL = 0,
    T_ERROR = 1,
    T_INTEGER,
    T_FLOAT,
    T_STRING,
    T_KEYWORD,
    T_IDENTIFIER,
    T_ARROW,
    T_FUNCARROW,
    T_COLONBRACE,
    T_PAAMAYIM_NEKUDOTAYIM,
    T_EOF = 255,
} sbTokenType;

typedef struct sbLexToken {
    sbTokenType type;
    union {
        char *str;
        float fl;
        int i;
    };
} sbLexToken;

hScanner sbScanner_create(hFileReader fr);

sbLexToken sbScanner_next(hScanner sc);

sbLexToken sbScanner_peek(hScanner sc);

void sbScanner_destroy(hScanner sc);
