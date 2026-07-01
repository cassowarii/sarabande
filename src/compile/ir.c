#include "compile/ir.h"

#include "parse/ast.h"
#include "data/symbol.h"

#include "mem/debug.h"

#include <stdarg.h>

typedef struct varmapentry {
  const char *name;
  usize name_len;
  sbIrVariable *var;
} varmapentry;

static sbIrChunk *compile_ast_function(hIrProgram ir, sbAst params, sbAst seqast);
static sbIrChunk *new_chunk(hIrProgram ir);
static void chunk_deinitialize(hIrChunk ck);
static void print_stmt(sbIrStmt *s);

void sbIrProgram_initialize(hIrProgram ir, usize initial_arena_size) {
  *ir = (sbIrProgram) {0};
  sbArena_initialize(&ir->arena, initial_arena_size);
  sbBuffer_initialize(&ir->chunks, 4096);
  sbBuffer_initialize(&ir->varmapping, 4096);
}

void sbIrProgram_deinitialize(hIrProgram ir) {
  usize nchunks = ir->chunks.size / sizeof(sbIrChunk*);
  for (usize i = 0; i < nchunks; i++) {
    sbIrChunk *ck = ((sbIrChunk**)ir->chunks.data)[i];
    chunk_deinitialize(ck);
  }

  sbArena_deinitialize(&ir->arena);
  sbBuffer_deinitialize(&ir->chunks);
  *ir = (sbIrProgram) {0};
}

void sbIrProgram_compile_ast(hIrProgram ir, sbAst ast) {
  /* compile a full program as like the body of a function
   * with no parameters */
  compile_ast_function(ir, NO_NODE, ast);
}

void sbIrProgram_print(hIrProgram ir) {
  usize nchunks = ir->chunks.size / sizeof(sbIrChunk*);

  for (usize i = 0; i < nchunks; i++) {
    sbIrChunk *ck = ((sbIrChunk**)ir->chunks.data)[i];
    debug("\nCHUNK %d with %d variables\n", ck->id, ck->variable_count);

    usize nstmts = ck->stmts.size / sizeof(sbIrStmt*);
    for (usize j = 0; j < nstmts; j++) {
      sbIrStmt *s = ((sbIrStmt**)ck->stmts.data)[j];
      print_stmt(s);
    }

    debug("\n");
  }
}

/* --- */

static sbIrExpr SENTINEL_NIL_EXPR = {
  .type = IR_E_VALUE,
  .value = (hV) { .type = IT_NIL }
};
static sbIrStmt SENTINEL_NO_STMT = {0};
static sbIrVariable SENTINEL_NO_VAR = {0};
static sbIrExpr *NIL_EXPR = &SENTINEL_NIL_EXPR;
static sbIrStmt *NO_STMT = &SENTINEL_NO_STMT;
static sbIrVariable *NO_VAR = &SENTINEL_NO_VAR;

static void vprogram_error(hIrProgram ir, const char *error, va_list args) {
  ir->error_count ++;
  fprintf(stderr, "error: ");
  vfprintf(stderr, error, args);
}

static void program_error(hIrProgram ir, const char *error, ...) {
  va_list args;
  va_start(args, error);
  vprogram_error(ir, error, args);
  va_end(args);
}

static void chunk_error(hIrChunk ck, const char *error, ...) {
  va_list args;
  va_start(args, error);
  vprogram_error(ck->program, error, args);
  va_end(args);
}

static sbIrChunk *new_chunk(hIrProgram ir) {
  usize nchunks = ir->chunks.size / sizeof(sbIrChunk*);
  sbIrChunk *chunk = sbArena_alloc(&ir->arena, sizeof(sbIrChunk));
  sbBuffer_initialize(&chunk->stmts, 1024);
  chunk->id = nchunks;
  chunk->program = ir;

  /* calculate how many variables are already in scope and then
   * store this info in the chunk. we can use this for calculating
   * which variables are upvalues, and also to know when certain
   * variable slots can be reused due to lexical scope */
  usize nvars = ir->varmapping.size / sizeof(varmapentry);
  chunk->lowest_var_id = nvars;

  sbBuffer_append(&ir->chunks, &chunk, sizeof(sbIrChunk*));
  return chunk;
}

static void chunk_deinitialize(hIrChunk ck) {
  sbBuffer_deinitialize(&ck->stmts);
}

/* we can introduce new variables into a scope using LET */
static sbIrVariable *create_var(hIrChunk ck, const char *name, usize name_len) {
  sbIrVariable *new_var = sbArena_alloc(&ck->program->arena, sizeof(sbIrVariable));

  usize nvars = ck->program->varmapping.size / sizeof(varmapentry);

  *new_var = (sbIrVariable) {0};
  new_var->slot_id = nvars - ck->lowest_var_id + 1;
  if (new_var->slot_id > ck->variable_count) {
    /* store max variable slot count used so we know how many
     * need to be allocated for our chunk */
    ck->variable_count = new_var->slot_id;
  }

  sbBuffer_append(&ck->program->varmapping, &(varmapentry) {
    .name = name,
    .name_len = name_len,
    .var = new_var,
  }, sizeof(varmapentry));

  return new_var;
}

/* find variables that already exist */
static sbIrVariable *var_name(hIrChunk ck, const char *name, usize name_len) {
  usize nvars = ck->program->varmapping.size / sizeof(varmapentry);
  for (usize i = ck->lowest_var_id; i < nvars; i++) {
    varmapentry *entry = &((varmapentry*)ck->program->varmapping.data)[i];
    if (sbstrncmp(name, entry->name, name_len) == 0) {
      return entry->var;
    }
  }

  /* TODO do a different thing here */
  chunk_error(ck, "unknown variable name! '%s'\n", name);
  return NO_VAR;
}

static sbIrLabel *new_label(hIrChunk ck) {
  sbIrLabel *l = sbArena_alloc(&ck->program->arena, sizeof(sbIrLabel));
  ck->label_count ++;
  l->id = ck->label_count;
  return l;
}

static sbIrExpr *new_expr(hIrChunk ck, sbIrExpr *expr) {
  sbIrExpr *where_to_put = sbArena_alloc(&ck->program->arena, sizeof(sbIrExpr));
  memcpy(where_to_put, expr, sizeof(sbIrExpr));
  return where_to_put;
}

static sbIrExpr *expr_var(hIrChunk ck, sbIrVariable *var) {
  return new_expr(ck, &(sbIrExpr) {
    .type = IR_E_VAR,
    .var = var,
  });
}

static sbIrExpr *expr_value(hIrChunk ck, hV *value) {
  return new_expr(ck, &(sbIrExpr) {
    .type = IR_E_VALUE,
    .value = *value,
  });
}

static sbIrExpr *expr_func(hIrChunk ck, sbIrChunk *func) {
  return new_expr(ck, &(sbIrExpr) {
    .type = IR_E_FUNC,
    .func = func,
  });
}

static flag int_constant_fold(sbAstOp op, hInteger left, hInteger right, hInteger *result) {
  switch (op) {
    case AST_OP_ADD:
      *result = left + right;
      break;
    case AST_OP_SUB:
      *result = left - right;
      break;
    case AST_OP_MUL:
      *result = left * right;
      break;
    case AST_OP_FLDIV:
      *result = left / right;
      break;
    case AST_OP_MOD:
      *result = left % right;
      break;
    case AST_OP_UNMINUS:
      *result = -left;
      break;
    default:
      return FALSE;
  }
  return TRUE;
}

static sbIrExpr *expr_op(hIrChunk ck, sbAstOp op, sbIrExpr *left, sbIrExpr *right) {
  if (left->type == IR_E_VALUE && left->value.type == IT_INTEGER
        && (!right || (right->type == IR_E_VALUE && right->value.type == IT_INTEGER))) {
    /* Try constant folding */
    hInteger result;
    flag folded = int_constant_fold(op, left->value.integer, right ? right->value.integer : 0, &result);
    if (folded) {
      return new_expr(ck, &(sbIrExpr) {
        .type = IR_E_VALUE,
        .value = HVINT(result),
      });
    }
  }

  return new_expr(ck, &(sbIrExpr) {
    .type = IR_E_OP,
    .op.type = op,
    .op.left = left,
    .op.right = right,
  });
}

static sbIrExpr *expr_call(hIrChunk ck, sbIrExpr *to_call, sbIrExpr *param) {
  return new_expr(ck, &(sbIrExpr) {
    .type = IR_E_CALL,
    .call.func = to_call,
    .call.param = param,
  });
}

static sbIrExpr *expr_list(hIrChunk ck, sbIrExpr *value) {
  return new_expr(ck, &(sbIrExpr) {
    .type = IR_E_LIST,
    .list.this = value,
  });
}

static sbIrExpr *expr_hash(hIrChunk ck, sbIrExpr *value) {
  return new_expr(ck, &(sbIrExpr) {
    .type = IR_E_HASH,
    .list.this = value,
  });
}

static void put_ir_stmt(hIrChunk ck, sbIrStmt *stmt) {
  sbIrStmt *where_to_put = sbArena_alloc(&ck->program->arena, sizeof(sbIrStmt));
  memcpy(where_to_put, stmt, sizeof(sbIrStmt));
  sbBuffer_append(&ck->stmts, &where_to_put, sizeof(sbIrStmt*));
}

static sbIrStmt *last_stmt(hIrChunk ck) {
  usize nstmts = ck->stmts.size / sizeof(sbIrStmt*);
  if (nstmts == 0) return NO_STMT;
  return ((sbIrStmt**)ck->stmts.data)[nstmts - 1];
}

static void erase_last_stmt(hIrChunk ck) {
  usize nstmts = ck->stmts.size / sizeof(sbIrStmt*);
  if (nstmts == 0) return;
  sbBuffer_set_size(&ck->stmts, (nstmts - 1) * sizeof(sbIrStmt*));
}

static void alias_label(sbIrLabel *l1, sbIrLabel *l2) {
  l1->aliased_to = l2;
}

static void put_label(hIrChunk ck, sbIrLabel *l) {
  if (last_stmt(ck)->type == IR_S_LABEL) {
    /* two labels in a row can be consolidated. this isn't really an optimization,
     * but it may improve consideration of other things */
    alias_label(l, last_stmt(ck)->label);
  }

  put_ir_stmt(ck, &(sbIrStmt) {
    .type = IR_S_LABEL,
    .label = l,
  });
}

static void put_jump(hIrChunk ck, flag inverted, sbIrExpr *condition, sbIrLabel *label) {
  if (last_stmt(ck)->type == IR_S_LABEL && condition == NULL) {
    /* optimization: a label immediately followed by an unconditional jump
     * means we can consider the label to be equivalent to wherever
     * the jump was to. */
    alias_label(last_stmt(ck)->label, label);
  }

  if (condition && condition->type == IR_E_OP && condition->op.type == AST_OP_NOT) {
    /* optimization: JUMP IF TRUE (NOT ...) can become JUMP IF FALSE (...) */
    inverted = !inverted;
    condition = condition->op.left;
  }

  put_ir_stmt(ck, &(sbIrStmt) {
    .type = IR_S_JUMP,
    .jump.inverted = inverted,
    .jump.condition = condition,
    .jump.label = label,
  });
}

static void put_jump_if_yes(hIrChunk ck, sbIrExpr *condition, sbIrLabel *label) {
  put_jump(ck, 0, condition, label);
}

static void put_jump_if_no(hIrChunk ck, sbIrExpr *condition, sbIrLabel *label) {
  put_jump(ck, 1, condition, label);
}

static void put_jump_unconditional(hIrChunk ck, sbIrLabel *label) {
  put_jump(ck, 0, NULL, label);
}

static void put_expr(hIrChunk ck, sbIrExpr *expr) {
  put_ir_stmt(ck, &(sbIrStmt) {
    .type = IR_S_EXPR,
    .expr = expr,
  });
}

static void print_expr(sbIrExpr *e);
static void put_assign(hIrChunk ck, sbIrVariable *assign_to, sbIrExpr *expr) {
  assign_to->initialized = TRUE;
  put_ir_stmt(ck, &(sbIrStmt) {
    .type = IR_S_ASSIGN,
    .assign.var = assign_to,
    .assign.expr = expr,
  });
}

static void put_argument(hIrChunk ck, sbIrVariable *v, flag last) {
  put_ir_stmt(ck, &(sbIrStmt) {
    .type = IR_S_ARG,
    .arg.last = last,
    .arg.var = v,
  });
}

static void put_return(hIrChunk ck, sbIrExpr *e) {
  put_ir_stmt(ck, &(sbIrStmt) {
    .type = IR_S_RETURN,
    .expr = e,
  });
}

static void put_implicit_return(hIrChunk ck) {
  sbIrStmt *last_s = last_stmt(ck);
  if (last_s->type == IR_S_RETURN) {
    /* don't put additional returns directly after a return,
     * that's silly */
    return;
  } else if (last_s->type == IR_S_EXPR) {
    erase_last_stmt(ck);
    put_return(ck, last_s->expr);
  } else {
    put_return(ck, NIL_EXPR);
  }
}

static void compile_ast_stmt(hIrChunk ck, sbAst stmtast, flag implicit_return);
static void compile_ast_stmtseq(hIrChunk ck, sbAst seqast, flag implicit_return);
static sbIrExpr *compile_ast_expr(hIrChunk ck, sbAst exprast);
static sbIrVariable *compile_ast_var(hIrChunk ck, sbAst node);

static sbIrChunk *compile_ast_function(hIrProgram ir, sbAst paramsAst, sbAst seqast) {
  sbIrChunk *ck = new_chunk(ir);

  /* create new scope to hold function parameters */
  usize parameter_scope_level = ck->program->varmapping.size / sizeof(varmapentry);

  sbBuffer arg_vars;
  sbBuffer_initialize(&arg_vars, 1024);
  sbAst param_comma = paramsAst;
  while (param_comma != NO_NODE) {
    if (param_comma->seq.left->type != AST_NODE_NAME) {
      /* TODO: Actually... */
      program_error(ir, "Only variable names are permitted as function parameters!");
      break;
    }
    const char *vname = sbSymbol_name(param_comma->seq.left->symb);
    sbIrVariable *v1 = create_var(ck, vname, strlen(vname));
    sbBuffer_append(&arg_vars, &v1, sizeof(sbIrVariable*));
    v1->initialized = TRUE;
    param_comma = param_comma->seq.right;
    ck->num_args ++;
  }

  while (arg_vars.size > 0) {
    /* We need to bind function arguments in reverse order that they are passed,
     * because arguments are going to be passed on a stack, and we want to evaluate
     * function arguments left-to-right. */
    sbIrVariable *var = *(sbIrVariable**)sbBuffer_shrink(&arg_vars, sizeof(sbIrVariable*));
    put_argument(ck, var, arg_vars.size == 0);
  }

  compile_ast_stmtseq(ck, seqast, TRUE);

  /* exit parameters' lexical scope */
  sbBuffer_set_size(&ck->program->varmapping, parameter_scope_level * sizeof(varmapentry));

  return ck;
}

static void compile_ast_stmtseq(hIrChunk ck, sbAst seqast, flag implicit_return) {
  /* create new scope to hold lexical block variables */
  usize lowest_var = ck->program->varmapping.size / sizeof(varmapentry);

  sbAst considering = seqast;
  while (considering != NO_NODE) {
    /* walk down the tree a line at a time */
    if (considering->type != AST_NODE_SEQ) {
      PANIC("compile_ast_stmtseq expects a SEQ node! (received instead #%d)", seqast->type);
    }

    flag is_last_stmt = (considering->seq.right == NO_NODE);

    compile_ast_stmt(ck, considering->seq.left, implicit_return && is_last_stmt);
    considering = considering->seq.right;

    if (ck->program->error_count > 0) {
      return;
    }
  }

  if (implicit_return) {
    put_implicit_return(ck);
  }

  /* remove extraneous statements, consolidate aliased labels */
  usize nstmts = ck->stmts.size / sizeof(sbIrStmt*);
  usize i = 0;
  usize j = 0;
  while (j < nstmts) {
    flag delete = FALSE;
    sbIrStmt *s = ((sbIrStmt**)ck->stmts.data)[i];
    if (s->type == IR_S_JUMP) {
      while (s->jump.label->aliased_to) {
        s->jump.label = s->jump.label->aliased_to;
      }
    } else if (s->type == IR_S_LABEL) {
      if (s->label->aliased_to) delete = TRUE;
    }

    if (delete) j++;
    if (j == nstmts) break;
    if (j > i) {
      /* deleting a statement would leave a gap, so keep track of
       * where we are with things that are deleted (j) and copy them
       * backwards to the current position (i) */
      sbIrStmt *t = ((sbIrStmt**)ck->stmts.data)[j];
      *s = *t;
    }

    i++;
    j++;
  }

  /* now, i is the number of statements we were left with after deleting some */
  sbBuffer_set_size(&ck->stmts, i * sizeof(sbIrStmt*));

  /* exit block lexical scope */
  sbBuffer_set_size(&ck->program->varmapping, lowest_var * sizeof(varmapentry));
}

static void compile_ast_stmt(hIrChunk ck, sbAst node, flag implicit_return) {
  sbIrExpr *E1;
  sbIrLabel *L1, *L2;
  sbIrVariable *V1;
  sbIrChunk *C1;
  sbAst N1, N2;
  switch (node->type) {
    case AST_NODE_RETURN:
      /*
       *    RETURN a
       * linearizes to:
       *    RETURN a
       * 
       * OK, this one's pretty simple.
       */
      if (node->seq.left == NO_NODE) {
        E1 = NULL;
      } else {
        /* TODO handle multival here */
        E1 = compile_ast_expr(ck, node->seq.left->seq.left);
      }
      put_return(ck, E1);
      break;

    case AST_NODE_LET:
      /*
       *    LET a = 3
       * linearizes to:
       *    LET a
       *    a = 3
       *
       *    LET a, b = 5, 6
       * linearizes to:
       *    LET a, b
       *    a = 5
       *    b = 6
       *
       * "LET a" doesn't actually appear in the output IR code,
       * but it allows us to interpret the name "a" inside the
       * current block to refer to a consistent variable slot.
       */
      if (node->seq.right == NO_NODE) {
        /* let ... */
        if (node->seq.left->type != AST_NODE_MULTIVAL) PANIC("expected multival on left side of let!");
        N1 = node->seq.left;  /* things to bind to */
        while (N1 != NO_NODE) {
          if (N1->seq.left->type != AST_NODE_NAME) {
            chunk_error(ck, "Only variable names are permitted after 'let'!");
            break;
          }
          const char *vname = sbSymbol_name(N1->seq.left->symb);
          V1 = create_var(ck, vname, strlen(vname));
          N1 = N1->seq.right;
        }
      } else {
        /* let ... = ... */
        if (node->seq.right->type != AST_NODE_MULTIVAL) PANIC("expected multival on right side of let!");
        N1 = node->seq.left;  /* things to bind to */
        while (N1 != NO_NODE) {
          /* create all variables first, then initialize them.
           * we can write something like "let q, r = 5, q + 1", but if we write something
           * like "let q, r = r + 1, 5", we should fail with "r is not initialized," not
           * "i don't know what r means". */
          if (N1->seq.left->type != AST_NODE_NAME) {
            chunk_error(ck, "Only variable names are permitted on the left side of 'let'!");
            break;
          }
          const char *vname = sbSymbol_name(N1->seq.left->symb);
          V1 = create_var(ck, vname, strlen(vname));
          N1 = N1->seq.right;
        }

        N1 = node->seq.left;  /* things to bind to */
        N2 = node->seq.right; /* values to assign */
        while (N1 != NO_NODE && N2 != NO_NODE) {
          /* now assign to all the variables we just created */
          const char *vname = sbSymbol_name(N1->seq.left->symb);
          V1 = var_name(ck, vname, strlen(vname));
          E1 = compile_ast_expr(ck, N2->seq.left);
          put_assign(ck, V1, E1);
          N1 = N1->seq.right;
          N2 = N2->seq.right;
        }

        if (N1 != NO_NODE) {
          chunk_error(ck, "too many bindings on left side of assignment!");
        } else if (N2 != NO_NODE) {
          chunk_error(ck, "too many values on right side of assignment!");
        }
      }
      break;

    case AST_NODE_ASSIGN:
      /*
       *    x, y, z = 1, 2, 3
       * linearizes to
       *    x = 1
       *    y = 2
       *    z = 3
       *
       * TODO: For things that are not straight variable names,
       * we need to convert them to the appropriate method calls.
       * Right now we don't have method calls, but eventually, we
       * will have them. */
      /* Probably we should just call out to a "compile_ast_assign"
       * function here which handles all that, and we could also call
       * it from 'let'. */
      if (node->seq.left->type != AST_NODE_MULTIVAL) PANIC("expected multival on left side of assign!");
      if (node->seq.right->type != AST_NODE_MULTIVAL) PANIC("expected multival on right side of assign!");
      N1 = node->seq.left;  /* things to bind to */
      N2 = node->seq.right; /* values to assign */
      while (N1 != NO_NODE && N2 != NO_NODE) {
        V1 = compile_ast_var(ck, N1->seq.left);
        E1 = compile_ast_expr(ck, N2->seq.left);
        put_assign(ck, V1, E1);
        N1 = N1->seq.right;
        N2 = N2->seq.right;
      }

      if (N1 != NO_NODE) {
        chunk_error(ck, "Too many bindings on left side of assignment");
      } else if (N2 != NO_NODE) {
        chunk_error(ck, "Too many values on right side of assignment");
      }
      break;

    case AST_NODE_DEF:
      /* DEF is like LET, except we are assigning a function value.
       * TODO: I still need to think about how to handle function
       * calling conventions / parameters with this variable system. */
      if (node->seq.left->type != AST_NODE_NAME) PANIC("expected name for function in def!");
      if (node->seq.right->type != AST_VAL_FUNC) PANIC("expected function body for function in def!");
      sbAst func_node = node->seq.right;
      if (func_node->seq.left->type != AST_NODE_MULTIVAL) PANIC("expected multival as function params!");
      if (func_node->seq.right->type != AST_NODE_SEQ) PANIC("expected SEQ node as function body!");

      const char *vname = sbSymbol_name(node->seq.left->symb);
      V1 = create_var(ck, vname, strlen(vname));
      C1 = compile_ast_function(ck->program, func_node->seq.left, func_node->seq.right);
      E1 = expr_func(ck, C1);
      put_assign(ck, V1, E1);
      break;

    case AST_NODE_WHILE:
      /*
       *    WHILE condition { things }
       * linearizes to:
       *    JUMP TO LABEL 2 UNCONDITIONALLY
       *    LABEL 1
       *    things
       *    LABEL 2
       *    JUMP TO LABEL 1 IF condition
       *
       * UNTIL is rewritten to WHILE NOT at the parsing stage.
       */
      /* TODO: 'break' and 'next' inside loops need
       * to be handled by passing in labels to this
       * stmtseq. If we have no labels, we're not in
       * a loop, and should error. */
      /* match continue / success jump can be handled
       * the same way. Maybe we should have a "labels
       * context" struct that captures all of these. */
      L1 = new_label(ck);
      L2 = new_label(ck);
      E1 = compile_ast_expr(ck, node->seq.left);
      put_jump_unconditional(ck, L2);
      put_label(ck, L1);
      compile_ast_stmtseq(ck, node->seq.right, FALSE);
      put_label(ck, L2);
      put_jump_if_yes(ck, E1, L1);
      break;

    case AST_NODE_IF:
      /*
       *    IF condition { things }
       * linearizes to:
       *    JUMP TO LABEL 1 IF NOT condition
       *    things
       *    LABEL 1
       *
       *    IF condition { things1 } ELSE { things2 }
       * linearizes to:
       *    JUMP TO LABEL 1 IF NOT condition
       *    things1
       *    JUMP TO LABEL 2 UNCONDITIONALLY
       *    LABEL 1
       *    things2
       *    LABEL 2
       *
       * UNLESS is rewritten to IF NOT at the parsing stage.
       */
      if (node->tri.right == NO_NODE) {
        /* no else branch */
        E1 = compile_ast_expr(ck, node->tri.left);
        L1 = new_label(ck);
        put_jump_if_no(ck, E1, L1);
        compile_ast_stmtseq(ck, node->tri.center, implicit_return);
        put_label(ck, L1);
      } else {
        /* yes else branch */
        E1 = compile_ast_expr(ck, node->tri.left);
        L1 = new_label(ck);
        L2 = new_label(ck);
        put_jump_if_no(ck, E1, L1);
        compile_ast_stmtseq(ck, node->tri.center, implicit_return);
        put_jump_unconditional(ck, L2);
        put_label(ck, L1);
        compile_ast_stmtseq(ck, node->tri.right, implicit_return);
        put_label(ck, L2);
      }
      break;

    case AST_NODE_REPEAT:
      /*
       *    REPEAT { things } WHILE condition
       * linearizes to:
       *    LABEL 1
       *    things
       *    JUMP TO LABEL 1 IF condition
       *
       *    REPEAT { things }
       * linearizes to:
       *    LABEL 1
       *    things
       *    JUMP TO LABEL 1 UNCONDITIONALLY
       *
       * REPEAT..UNTIL is rewritten to REPEAT..WHILE NOT
       * at the parsing stage.
       */
      L1 = new_label(ck);
      if (node->seq.right) {
        E1 = compile_ast_expr(ck, node->seq.right);
        put_label(ck, L1);
        compile_ast_stmtseq(ck, node->seq.left, FALSE);
        put_jump_if_yes(ck, E1, L1);
      } else {
        put_label(ck, L1);
        compile_ast_stmtseq(ck, node->seq.left, FALSE);
        put_jump_unconditional(ck, L1);
      }
      break;

    default:
      /* probably an expr node */
      put_expr(ck, compile_ast_expr(ck, node));
  }
}

static sbIrVariable *compile_ast_var(hIrChunk ck, sbAst node) {
  if (node->type == AST_NODE_NAME) {
    return var_name(ck, sbSymbol_name(node->symb), strlen(sbSymbol_name(node->symb)));
  } else {
    /* TODO make errors not bad */
    chunk_error(ck, "Only a variable name is permitted here. (got %d)\n", node->type);
    return NO_VAR;
  }
}

static sbIrExpr *compile_ast_list(hIrChunk ck, sbAst node) {
  sbAst considering = node;
  sbIrExpr *list = NULL;
  sbIrExpr **place_here = &list;
  while (considering != NO_NODE) {
    *place_here = expr_list(ck, compile_ast_expr(ck, considering->seq.left));
    place_here = &(*place_here)->list.next;
    considering = considering->seq.right;
  }
  return list;
}

static sbIrExpr *compile_ast_hash(hIrChunk ck, sbAst node) {
  sbAst considering = node;
  sbIrExpr *list = NULL;
  sbIrExpr **place_here = &list;
  while (considering != NO_NODE) {
    /* we'll just make a list of "lists" that are actually just cons-cells of a key and value */
    sbAst hash_entry = considering->seq.left;
    if (hash_entry->type != AST_NODE_HASHENTRY) {
      PANIC("Hash table literals should only contain hashentries (%d)", hash_entry->type);
    }
    sbIrExpr *compiled_entry = expr_list(ck, compile_ast_expr(ck, hash_entry->seq.left)); // key
    compiled_entry->list.next = compile_ast_expr(ck, hash_entry->seq.right); // value

    *place_here = expr_hash(ck, compiled_entry);
    place_here = &(*place_here)->list.next;
    considering = considering->seq.right;
  }
  return list;
}

static sbIrExpr *compile_ast_expr(hIrChunk ck, sbAst node) {
  if (node == NO_NODE) return NULL;

  if (node->type == AST_NODE_FUNCCALL) {
    sbIrExpr *called = compile_ast_expr(ck, node->seq.left);
    sbIrExpr *params = compile_ast_list(ck, node->seq.right);
    return expr_call(ck, called, params);
  } else if (node->type == AST_VAL_FUNC) {
    sbIrChunk *func = compile_ast_function(ck->program, node->seq.left, node->seq.right);
    return expr_func(ck, func);
  } else if (node->type == AST_VAL_LIST) {
    sbIrExpr *list = compile_ast_list(ck, node->seq.left);
    return list;
  } else if (node->type == AST_VAL_HASH) {
    sbIrExpr *hash = compile_ast_hash(ck, node->seq.left);
    return hash;
  } else if (node->type == AST_NODE_OP) {
    sbIrExpr *left = NULL, *right = NULL;
    if (node->op.left != NO_NODE) {
      left = compile_ast_expr(ck, node->op.left);
    }
    if (node->op.right != NO_NODE) {
      right = compile_ast_expr(ck, node->op.right);
    }
    return expr_op(ck, node->op.type, left, right);
  } else if (node->type == AST_VAL_INT) {
    return expr_value(ck, &HVINT(node->i));
  } else if (node->type == AST_VAL_STRING) {
    return expr_value(ck, &HVSTR(node->str));
  } else if (node->type == AST_VAL_SYMBOL) {
    return expr_value(ck, &HVSYM(node->symb));
  } else if (node->type == AST_NODE_NAME) {
    /* TODO: I don't want to use strlen. Can we remember the lengths of symbols? */
    sbIrVariable *var = compile_ast_var(ck, node);
    if (var->initialized) {
      return expr_var(ck, var);
    } else {
      chunk_error(ck, "Variable '%s' has not yet been initialized here!\n", sbSymbol_name(node->symb));
      return NULL;
    }
  } else {
    PANIC("expr todo! (%d)", node->type);
  }
}

static void print_expr(sbIrExpr *e) {
  debug("(");
  switch(e->type) {
    case IR_E_VAR:
      debug("variable %zu", e->var->slot_id);
      break;
    case IR_E_FUNC:
      debug("{ chunk %d }", e->func->id);
      break;
    case IR_E_VALUE:
      if (e->value.type == IT_NIL) {
        debug("nil");
      } else if (e->value.type == IT_INTEGER) {
        debug("%lld", e->value.integer);
      } else {
        debug("some value");
      }
      break;
    case IR_E_OP:
      print_expr(e->op.left);
      debug(" %c ", e->op.type);
      if (e->op.right) {
        print_expr(e->op.right);
      }
      break;
    case IR_E_CALL:
      debug("CALL: ");
      print_expr(e->call.func);
      debug(" with params ");
      sbIrExpr *param = e->call.param;
      while (param) {
        print_expr(param->list.this);
        debug(",");
        param = param->list.next;
      }
      break;
    default:
      debug("something");
  }
  debug(")");
}

static void print_stmt(sbIrStmt *s) {
  switch (s->type) {
    case IR_S_ARG:
      debug("  bind function argument to variable %zu\n", s->arg.var->slot_id);
      if (s->arg.last) {
        debug("  (no function arguments left on the stack now)\n");
      }
      break;
    case IR_S_LABEL:
      debug("label %zu:\n", s->label->id);
      break;
    case IR_S_JUMP:
      debug("  jump to label %zu", s->jump.label->id);
      if (s->jump.condition) {
        if (s->jump.inverted) {
          debug(" unless ");
        } else {
          debug(" if ");
        }
        print_expr(s->jump.condition);
      }
      debug("\n");
      break;
    case IR_S_RETURN:
      debug("  return ");
      if (s->expr) {
        print_expr(s->expr);
      } else {
        debug("NOTHING");
      }
      debug("\n");
      break;
    case IR_S_EXPR:
      debug("  ");
      print_expr(s->expr);
      debug("\n");
      break;
    case IR_S_ASSIGN:
      debug("  variable %zu = ", s->assign.var->slot_id);
      print_expr(s->assign.expr);
      debug("\n");
      break;
    default:
      debug("  (do something)\n");
  }
}
