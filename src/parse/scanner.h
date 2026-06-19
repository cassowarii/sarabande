#include "common.h"

#include "filereader.h"

typedef struct sbScanner *hScanner;

typedef enum sbTokenType {
    T_LPAREN = '(',
    T_RPAREN = ')',
    T_LBRACKET = '[',
    T_RBRACKET = ']',
    T_LBRACE = '{',
    T_RBRACE = '}',
    T_ASTERISK = '*',
    T_SLASH = '/',
    T_PLUS = '+',
    T_MINUS = '-',
    T_PERCENT = '%',
    T_PIPE = '|',
    T_DOT = '.',
    T_COMMA = ',',
    T_EQUALS = '=',
    T_COLON = ':',
    T_SEMICOLON = ';',
    T_BACKSLASH = '\\',
    T_NEWLINE = '\n',
    T_SPACE = ' ',
    T_NULL = 0,
    T_ERROR = 1,
    T_KEYWORD,
    T_IDENTIFIER,
    T_SYMBOL,
    T_INTEGER,
    T_FLOAT,
    T_STRING,
    T_ARROW,
    T_FATARROW,
    T_COLONBRACE,
    T_DOUBLEEQUALS,
    T_DOUBLEMINUS,
    T_DOUBLEPLUS,
    T_DOUBLEASTERISK,
    T_DOUBLESLASH,
    T_MINUSEQUALS,
    T_PLUSEQUALS,
    T_ASTERISKEQUALS,
    T_SLASHEQUALS,
    T_PERCENTEQUALS,
    T_PAAMAYIM_NEKUDOTAYIM,
    T_EOF = 255,
} sbTokenType;

typedef struct sbLexToken {
    sbTokenType type;
    usize size;
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
