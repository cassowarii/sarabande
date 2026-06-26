#include "common.h"

#include "data/value.h"
#include "parse/token.h"

typedef enum sbAstType {
  AST_NULL,
  AST_VAL_NIL,
  AST_VAL_INT,
  AST_VAL_STRING,
  AST_VAL_FLOAT,
  AST_VAL_SYMBOL,
  AST_VAL_BOOLEAN,
  AST_NODE_NAME,
  AST_NODE_SEQ,
  AST_NODE_OP,
  AST_NODE_DEF,
  AST_NODE_LET,
  AST_NODE_IF,
  AST_NODE_THENELSE,
  AST_NODE_WHILE,
  AST_NODE_LIST,
  AST_NODE_HASH,
  AST_NODE_HASHENTRY,
  AST_NODE_NEXT,
  AST_NODE_GROUPING,
  AST_NODE_MULTIVAL,
  AST_NODE_FUNCCALL,
  AST_NODE_METHODCALL,
  AST_NODE_SEND,
  AST_NODE_ELLIPSIS,
} sbAstType;

typedef enum sbAstOp {
  AST_OP_NULL,
  AST_OP_PLUS = '+',
  AST_OP_MINUS = '-',
  AST_OP_TIMES = '*',
  AST_OP_DIV = '/',
  AST_OP_MOD = '%',
  AST_OP_EQ = '=',
  AST_OP_LT = '<',
  AST_OP_GT = '>',
  AST_OP_LE = 128,
  AST_OP_GE,
  AST_OP_NE,
  AST_OP_FLDIV,
  AST_OP_DIVBY,
  AST_OP_PN,
} sbAstOp;

typedef struct sbAstNode {
  sbAstType type;
  union {
    hString str;
    hSymbol symb;
    hInteger i;
    double fl;
    struct {
      const struct sbAstNode *this;
      const struct sbAstNode *next;
    } seq;
    struct {
      sbTokenType type;
      const struct sbAstNode *left;
      const struct sbAstNode *right;
    } op;
  };
} sbAstNode;

typedef const sbAstNode *sbAst;
