#ifndef __SB_TOKEN_H__
#define __SB_TOKEN_H__

typedef enum sbTokenType {
    T_NULL                  = 0,
    T_ERROR                 = 1,
    T_LPAREN                = '(',
    T_RPAREN                = ')',
    T_LBRACKET              = '[',
    T_RBRACKET              = ']',
    T_LBRACE                = '{',
    T_RBRACE                = '}',
    T_ASTERISK              = '*',
    T_SLASH                 = '/',
    T_PLUS                  = '+',
    T_MINUS                 = '-',
    T_PERCENT               = '%',
    T_PIPE                  = '|',
    T_DOT                   = '.',
    T_COMMA                 = ',',
    T_EQUALS                = '=',
    T_LESS                  = '<',
    T_GREATER               = '>',
    T_COLON                 = ':',
    T_SEMICOLON             = ';',
    T_BACKSLASH             = '\\',
    T_NEWLINE               = '\n',
    T_SPACE                 = ' ',
    T_IDENTIFIER            = 128, // abc
    T_SYMBOL,               // :abc
    T_INTEGER,              // 123 0xd00d 0b01101 0o644
    T_FLOAT,                // 0.3 1e3 5200.04
    T_STRING,               // "abc" 'abc' `abc`
    T_ARROW,                // ->
    T_FATARROW,             // =>
    T_SQUIGARROW,           // ~>
    T_BACKSQUIGARROW,       // <~
    T_COLONBRACE,           // :{
    T_DOUBLEEQUALS,         // ==
    T_DOUBLEMINUS,          // --
    T_DOUBLEPLUS,           // ++
    T_DOUBLEASTERISK,       // **
    T_DOUBLESLASH,          // //
    T_DOUBLEGREATER,        // >>
    T_DOUBLELESS,           // <<
    T_MINUSEQUALS,          // -=
    T_PLUSEQUALS,           // +=
    T_ASTERISKEQUALS,       // *=
    T_SLASHEQUALS,          // /=
    T_PERCENTEQUALS,        // %=
    T_LESSEQUALS,           // <=
    T_GREATEREQUALS,        // >=
    T_NOTEQUALS,            // !=
    T_PAAMAYIM_NEKUDOTAYIM, // ::

    /* reserved words */
    T_rAND,                 // and
    T_rAS,                  // as
    T_rCASE,                // case
    T_rDEF,                 // def
    T_rDO,                  // do
    T_rELSE,                // else
    T_rIF,                  // if
    T_rIN,                  // in
    T_rLET,                 // let
    T_rNOT,                 // not
    T_rOR,                  // or
    T_rREPEAT,              // repeat
    T_rRETURN,              // return
    T_rUNLESS,              // unless
    T_rUNTIL,               // until
    T_rWHEN,                // when
    T_rWHILE,               // while

    T_EOF                   = 255,
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
