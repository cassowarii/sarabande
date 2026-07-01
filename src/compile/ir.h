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
  IR_S_ARG,
  IR_S_RETURN,
} sbIrStmtType;

typedef enum sbIrExprType {
  IR_E_NULL,
  IR_E_VALUE,
  IR_E_OP,
  IR_E_VAR,
  IR_E_FUNC,
  IR_E_CALL,
  IR_E_LIST,
} sbIrExprType;

typedef struct sbIrLabel {
  flag found_yet;
  struct sbIrLabel *aliased_to;
  usize id;
  usize block_position;
} sbIrLabel;

typedef struct sbIrVariable {
  usize slot_id;
  flag initialized;
  usize first_appearance;
  usize last_appearance;
  usize assigned_index;
} sbIrVariable;

typedef struct sbIrJump {
  flag inverted;
  struct sbIrExpr *condition;
  sbIrLabel *label;
} sbIrJump;

typedef struct sbIrExpr {
  sbIrExprType type;
  union {
    hV value;
    struct sbIrChunk *func;
    sbIrVariable *var;
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
  };
} sbIrExpr;

typedef struct sbIrAssign {
  sbIrVariable *var;
  sbIrExpr *expr;
} sbIrAssign;

typedef struct sbIrStmt {
  i32 position;
  sbIrStmtType type;
  union {
    sbIrExpr *expr;
    sbIrLabel *label;
    sbIrAssign assign;
    sbIrJump jump;
    struct {
      flag last;
      sbIrVariable *var;
    } arg;
  };
} sbIrStmt;

struct sbIrProgram;

typedef struct sbIrChunk {
  struct sbIrProgram *program;
  i32 id;
  i16 num_args;
  flag variadic;
  i32 label_count;
  i32 variable_count;
  i32 lowest_var_id;
  sbBuffer stmts;
} sbIrChunk;

typedef struct sbIrProgram {
  sbArena arena;
  sbBuffer varmapping;
  sbBuffer chunks;
  i32 error_count;
} sbIrProgram;

typedef struct sbIrProgram *hIrProgram;

typedef struct sbIrChunk *hIrChunk;

void sbIrProgram_initialize(hIrProgram ir, usize initial_arena_size);

void sbIrProgram_deinitialize(hIrProgram ir);

void sbIrProgram_compile_ast(hIrProgram ir, sbAst ast);

void sbIrProgram_print(hIrProgram ir);

#endif
