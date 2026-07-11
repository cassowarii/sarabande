#include "compile/analyze.h"

#include "parse/ast.h"
#include "data/symbol.h"

i32 sbAst_count_pipe_underscores(sbAst node) {
  if (node == NULL) return 0;

  switch (node->type) {
    case AST_NULL:
      /* like the end of a list or whatever */
      return 0;
    case AST_NODE_NAME:
      if (sbstrncmp(sbSymbol_name(node->symb), "_", 1) == 0) {
        return 1;
      } else {
        return 0;
      }
    case AST_NODE_SEQ:
    case AST_NODE_NEXT:
    case AST_NODE_MULTIVAL:
    case AST_NODE_FUNCCALL:
    case AST_VAL_FUNC:
    case AST_VAL_IMFUNC:
    case AST_VAL_OBJ:
    case AST_NODE_HASHENTRY:
      return sbAst_count_pipe_underscores(node->seq.left)
           + sbAst_count_pipe_underscores(node->seq.right);
    case AST_NODE_OP:
      if (node->op.type == AST_OP_PIPE) {
        /* for a pipe, underscores on the right side don't count
         * towards this, because they refer to the result of whatever
         * is on the left side. but underscores on the left side do
         * refer to what we're binding, so they do count */
        return sbAst_count_pipe_underscores(node->op.left);
      } else {
        return sbAst_count_pipe_underscores(node->op.left);
             + sbAst_count_pipe_underscores(node->op.right);
      }
    case AST_NODE_ASSIGN:
    case AST_NODE_LET:
      /* for declarations and assignments, we only count the expressions
       * on the right, not the things on the left (this might happen if we
       * have a pipe into a function literal that contains statements etc) */
      return sbAst_count_pipe_underscores(node->op.right);
    case AST_VAL_LIST:
    case AST_VAL_HASH:
      /* check the things inside the whatever */
      return sbAst_count_pipe_underscores(node->op.left);
    case AST_VAL_NIL:
    case AST_VAL_INT:
    case AST_VAL_STRING:
    case AST_VAL_FLOAT:
    case AST_VAL_SYMBOL:
    case AST_VAL_BOOLEAN:
      /* these are not the name "_" */
      return 0;
    default:
      PANIC("aah! (%lld)", (long long)node->type);
  }
}
