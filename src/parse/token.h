#ifndef __SB_TOKEN_H__
#define __SB_TOKEN_H__

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
    T_LESS = '<',
    T_GREATER = '>',
    T_COLON = ':',
    T_SEMICOLON = ';',
    T_BACKSLASH = '\\',
    T_NEWLINE = '\n',
    T_SPACE = ' ',
    T_NULL = 0,
    T_ERROR = 1,
    T_KEYWORD = 128,
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
    T_DOUBLEGREATER,
    T_DOUBLELESS,
    T_MINUSEQUALS,
    T_PLUSEQUALS,
    T_ASTERISKEQUALS,
    T_SLASHEQUALS,
    T_PERCENTEQUALS,
    T_LESSEQUALS,
    T_GREATEREQUALS,
    T_NOTEQUALS,
    T_PAAMAYIM_NEKUDOTAYIM,

    /* reserved words */
    T_rAND,
    T_rAS,
    T_rCASE,
    T_rDEF,
    T_rDO,
    T_rELSE,
    T_rIF,
    T_rIN,
    T_rLET,
    T_rNOT,
    T_rOR,
    T_rREPEAT,
    T_rRETURN,
    T_rUNLESS,
    T_rUNTIL,
    T_rWHEN,
    T_rWHILE,

    T_EOF = 255,
} sbTokenType;

typedef struct sbLexToken {
    sbTokenType type;
    usize size;
    flag invisible;
    union {
        char *str;
        float fl;
        int i;
    };
} sbLexToken;

#endif
