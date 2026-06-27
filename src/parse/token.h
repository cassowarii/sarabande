#ifndef __SB_TOKEN_H__
#define __SB_TOKEN_H__

#include "common.h"

#include "data/value.h"

typedef enum sbTokenType {
    T_NULL                  = 0,
    T_ERROR                 = 1,
    T_LPAREN                = '(', // op::call
    T_RPAREN                = ')',
    T_LBRACKET              = '[', // op::index
    T_RBRACKET              = ']',
    T_LBRACE                = '{',
    T_RBRACE                = '}',
    T_ASTERISK              = '*', // op::mul
    T_SLASH                 = '/', // op::div
    T_PLUS                  = '+', // op::add
    T_MINUS                 = '-', // op::sub
    T_PERCENT               = '%', // op::mod
    T_PIPE                  = '|',
    T_DOT                   = '.',
    T_COMMA                 = ',',
    T_EQUALS                = '=',
    T_LESS                  = '<', // op::lt
    T_GREATER               = '>', // op::gt
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
    T_PAAMAYIM_NEKUDOTAYIM, // :: op::scope
    T_DOUBLEEQUALS,         // == op::eq
    T_DOUBLEMINUS,          // -- op::decr
    T_DOUBLEPLUS,           // ++ op::incr
    T_DOUBLEASTERISK,       // ** op::pow
    T_DOUBLESLASH,          // // op::floordiv
    T_DOUBLEPERCENT,        // %% op::divisible
    T_DOUBLEGREATER,        // >> op::rshift
    T_DOUBLELESS,           // << op::lshift
    T_MINUSEQUALS,          // -=
    T_PLUSEQUALS,           // +=
    T_ASTERISKEQUALS,       // *=
    T_SLASHEQUALS,          // /=
    T_PERCENTEQUALS,        // %=
    T_LESSEQUALS,           // <= op::le
    T_GREATEREQUALS,        // >= op::ge
    T_NOTEQUALS,            // != op::ne
    T_TWODOT,               // ..
    T_ELLIPSIS,             // ...

    /* reserved words */
    T_rAND,                 // and
    T_rAS,                  // as
    T_rCASE,                // case
    T_rDEF,                 // def
    T_rDO,                  // do
    T_rELSE,                // else
    T_rFALSE,               // false
    T_rIF,                  // if
    T_rIN,                  // in
    T_rLET,                 // let
    T_rMATCH,               // match
    T_rNIL,                 // nil
    T_rNOT,                 // not
    T_rOR,                  // or
    T_rREPEAT,              // repeat
    T_rRETURN,              // return
    T_rTRUE,                // true
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
        char *cstr;
        hString hstr;
        hSymbol symb;
        float fl;
        int i;
    };
} sbLexToken;

typedef struct sbTokenQueue {
    sbBuffer buffer;
} sbTokenQueue;

typedef sbTokenQueue *hTokenQueue;

void sbTokenQueue_initialize(hTokenQueue q, i16 initial_size);

void sbTokenQueue_enqueue(hTokenQueue q, sbLexToken t);

sbLexToken sbTokenQueue_shift(hTokenQueue q);

i32 sbTokenQueue_size(hTokenQueue q);

sbLexToken sbTokenQueue_at(hTokenQueue q, i32 index);

void sbTokenQueue_deinitialize(hTokenQueue q);

#endif
