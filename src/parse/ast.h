#ifndef __SARABANDE_AST_H__
#define __SARABANDE_AST_H__

#include "common.h"

#include "data/value.h"
#include "parse/token.h"

typedef enum sbAstType {
  AST_NULL,
  AST_ERROR,
  AST_VAL_NIL,
  AST_VAL_INT,
  AST_VAL_STRING,
  AST_VAL_FLOAT,
  AST_VAL_SYMBOL,
  AST_VAL_BOOLEAN,
  AST_VAL_LIST,
  AST_VAL_HASH,
  AST_VAL_FUNC,
  AST_VAL_IMFUNC,
  AST_VAL_OBJ,
  AST_NODE_NAME,
  AST_NODE_SEQ,
  AST_NODE_OP,
  AST_NODE_DEF,
  AST_NODE_LET,
  AST_NODE_ASSIGN,
  AST_NODE_IF,
  AST_NODE_THENELSE,
  AST_NODE_WHILE,
  AST_NODE_REPEAT,
  AST_NODE_CASE,
  AST_NODE_MATCH,
  AST_NODE_LIST,
  AST_NODE_HASHENTRY,
  AST_NODE_NEXT,
  AST_NODE_MULTIVAL,
  AST_NODE_FUNCCALL,
  AST_NODE_METHODCALL,
  AST_NODE_SEND,
  AST_NODE_RETURN,
  AST_NODE_ELLIPSIS,
} sbAstType;

typedef enum sbAstOp {
  AST_OP_NULL,
  AST_OP_ADD = '+',
  AST_OP_SUB = '-',
  AST_OP_MUL = '*',
  AST_OP_DIV = '/',
  AST_OP_MOD = '%',
  AST_OP_EQ = '=',
  AST_OP_LT = '<',
  AST_OP_GT = '>',
  AST_OP_REF = '&',
  AST_OP_DEREF = '*',
  AST_OP_PIPE = '|',
  AST_OP_LE = 128,
  AST_OP_GE,
  AST_OP_NE,
  AST_OP_FLDIV,
  AST_OP_POW,
  AST_OP_INCR,
  AST_OP_DECR,
  AST_OP_RANGE,
  AST_OP_DIVBY,
  AST_OP_INDEX,
  AST_OP_SCOPE,
  AST_OP_UNPLUS,
  AST_OP_UNMINUS,
  AST_OP_OR,
  AST_OP_AND,
  AST_OP_NOT,
  AST_OP_IN,
  AST_OP_SEND,
  AST_OP_SPLAT,
} sbAstOp;

typedef struct sbAstNode {
  sbAstType type;
  union {
    hString str;
    hSymbol symb;
    hInteger i;
    double fl;
    struct {
      struct sbAstNode *left;
      struct sbAstNode *right;
    } seq;
    struct {
      sbAstOp type;
      struct sbAstNode *left;
      struct sbAstNode *right;
    } op;
    struct {
      struct sbAstNode *left;
      struct sbAstNode *center;
      struct sbAstNode *right;
    } tri;
  };
} sbAstNode;

typedef sbAstNode *sbAst;

extern sbAst NO_NODE;
extern sbAst ERROR_NODE;

#endif
