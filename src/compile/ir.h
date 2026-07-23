#ifndef __SARABANDE_IRTYPES_H__
#define __SARABANDE_IRTYPES_H__

#include "common.h"

#include "parse/ast.h"

/* Phase 5 of the compiler.
 * (The first four phases are in the `parse` module.)
 * Now that we made it past the harrowing syntax phase, we are
 * ready to start actually trying to compile the code. */

/* Convert AST parse tree to a "linearized" series of chunks
 * consisting of simple operations like assignments and jumps.
 * Expressions are still nested (we will flatten them in the next
 * phase.) Variables and labels / jump targets are still left
 * "blank" -- we will decide "where" they go once we have a whole
 * program to look at. */

/* One "chunk" in IR here should correspond to one "block" in the
 * VM module, with the same semantics: you can do local jumps
 * inside them, but jumping between them is due to call / return.
 * Each one represents a function body; we separate nested functions
 * out into their own things, and refer to them by ID. */

/* We will also do semantic analysis here, but since it is a dynamic
 * language, semantic analysis is fairly weak; in particular, we do
 * detect when variables are not in the current scope, but we assume
 * they exist via magical context and will show up at runtime. I sure
 * hope so, at least! */

typedef enum sbIrStmtType {
  IR_S_NULL,
  IR_S_LABEL,
  IR_S_EXPR,
  IR_S_ASSIGN,
  IR_S_JUMP,
  IR_S_BIND,
  IR_S_RETURN,
} sbIrStmtType;

typedef enum sbIrExprType {
  IR_E_NULL,
  IR_E_VALUE,
  IR_E_OP,
  IR_E_VAR,
  IR_E_CONTEXT,
  IR_E_FUNC,
  IR_E_CALL,
  IR_E_DOT,
  IR_E_LIST,
  IR_E_HASH,
} sbIrExprType;

typedef enum sbIrNameIntroduceType {
  NOT_INTRODUCED,
  BY_DEF,
  BY_LET,
  BY_PARAM,
  BY_CONTEXT,
  BY_IMPLICIT,
} sbIrNameIntroduceType;

typedef struct sbIrLabel {
  flag found_yet : 1;
  i32 id : 31;
  struct sbIrLabel *aliased_to;
  i32 block_position;
} sbIrLabel;

typedef struct sbIrVariable {
  flag is_reference : 1;
  flag is_upvalue : 1;
  sbIrNameIntroduceType introduced : 4;
  i32 mapping_index : 26;
  i32 slot_id;
} sbIrVariable;

typedef struct sbIrJump {
  flag inverted;
  struct sbIrExpr *condition;
  sbIrLabel *label;
} sbIrJump;

typedef struct sbIrExpr {
  sbIrExprType type;
  union {
    hVal value;
    sbIrVariable *var;
    hSymbol symbol;
    struct {
      struct sbIrChunk *chunk;
      sbBuffer bound;
    } func;
    struct {
      sbAstOp type;
      struct sbIrExpr *left;
      struct sbIrExpr *right;
    } op;
    struct {
      struct sbIrExpr *func;
      struct sbIrExpr *param;
    } call;
    struct {
      struct sbIrExpr *this;
      struct sbIrExpr *next;
    } list;
    struct {
      struct sbIrExpr *target;
      struct sbIrExpr *param;
    } dot;
  };
} sbIrExpr;

typedef struct sbIrAssign {
  sbIrExpr *where;
  sbIrExpr *value;
} sbIrAssign;

typedef struct sbIrBindList {
  usize pre_splat_count;
  sbIrExpr *this;
  struct sbIrBindList *next;
} sbIrBindList;

typedef struct sbIrStmt {
  i32 position;
  sbIrStmtType type;
  union {
    sbIrExpr *expr;
    sbIrLabel *label;
    sbIrAssign assign;
    sbIrJump jump;
    struct {
      sbIrBindList *vars;
      sbIrExpr *values;
    } bind;
  };
} sbIrStmt;

struct sbIrProgram;

typedef struct sbIrChunk {
  struct sbIrProgram *program;
  flag pipe_var_in_use : 1;
  i32 id : 31;
  i16 num_args;
  i16 num_upvalues;
  i32 label_count;
  i32 variable_count;
  i32 lowest_var_id;
  i32 pipe_var_id;
  sbBuffer closed_vars;
  sbBuffer stmts;
} sbIrChunk;

typedef struct sbIrProgram {
  sbArena arena;
  sbBuffer varmapping;
  sbBuffer chunks;
  sbBuffer buffers;
  sbBuffer context_vars;
  i32 error_count;
} sbIrProgram;

extern sbIrExpr *IR_EMPTY_LIST;

typedef struct sbIrProgram *hIrProgram;

typedef struct sbIrChunk *hIrChunk;

void sbIrProgram_initialize(hIrProgram ir, usize initial_arena_size);

void sbIrProgram_deinitialize(hIrProgram ir);

void sbIrProgram_compile_ast(hIrProgram ir, sbAst ast);

void sbIrProgram_print(hIrProgram ir);

void sbIr_print_expr(sbIrExpr *e);

#endif
