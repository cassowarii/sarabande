#include "compile/ir.h"

#include "parse/ast.h"
#include "data/symbol.h"

#include "mem/debug.h"

#include <stdarg.h>

#define BY_LET 1
#define BY_DEF 2
#define BY_PARAM 3

typedef struct varmapentry {
  const char *name;
  usize name_len;
  sbIrVariable *var;
} varmapentry;

static sbIrChunk *compile_ast_function(hIrProgram ir, sbAst params, sbAst seqast);
static sbIrChunk *new_chunk(hIrProgram ir);
static void chunk_deinitialize(hIrChunk ck);

void sbIrProgram_initialize(hIrProgram ir, usize initial_arena_size) {
  *ir = (sbIrProgram) {0};
  sbArena_initialize(&ir->arena, initial_arena_size);
  sbBuffer_initialize(&ir->chunks, 4096);
  sbBuffer_initialize(&ir->varmapping, 4096);
  sbBuffer_initialize(&ir->buffers, 4096);
}

void sbIrProgram_deinitialize(hIrProgram ir) {
  usize nchunks = ir->chunks.size / sizeof(sbIrChunk*);
  for (usize i = 0; i < nchunks; i++) {
    sbIrChunk *ck = ((sbIrChunk**)ir->chunks.data)[i];
    chunk_deinitialize(ck);
  }

  sbBuffer_deinitialize(&ir->chunks);
  sbBuffer_deinitialize(&ir->varmapping);
  BUFFER_ITER(ir->buffers, sbBuffer*, buf) {
    sbBuffer_deinitialize(*buf);
  }
  sbBuffer_deinitialize(&ir->buffers);
  sbArena_deinitialize(&ir->arena);

  *ir = (sbIrProgram) {0};
}

void sbIrProgram_compile_ast(hIrProgram ir, sbAst ast) {
  /* compile a full program as like the body of a function
   * with no parameters */
  compile_ast_function(ir, NO_NODE, ast);
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

static void chunk_error(hIrChunk ck, const char *error, ...) {
  va_list args;
  va_start(args, error);
  vprogram_error(ck->program, error, args);
  va_end(args);
}

static void init_buffer(hIrProgram ir, sbBuffer *b, usize capacity) {
  sbBuffer_initialize(b, capacity);
  sbBuffer_append(&ir->buffers, &b, sizeof(sbBuffer*));
}

static sbIrChunk *new_chunk(hIrProgram ir) {
  usize nchunks = ir->chunks.size / sizeof(sbIrChunk*);
  sbIrChunk *chunk = sbArena_alloc(&ir->arena, sizeof(sbIrChunk));
  sbBuffer_initialize(&chunk->stmts, 1024);
  sbBuffer_initialize(&chunk->closed_vars, 256);
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
  sbBuffer_deinitialize(&ck->closed_vars);
}

/* find variables that already exist */
static sbIrVariable *existing_var(hIrChunk ck, const char *name, usize name_len, usize *index_out) {
  usize index = 0;
  BUFFER_ITER(ck->program->varmapping, varmapentry, entry) {
    if (sbstrncmp(name, entry->name, name_len) == 0) {
      if (index_out) *index_out = index;
      return entry->var;
    }
    index ++;
  }
  return NO_VAR;
}

/* Create an 'upvalue' variable that refers to a normal variable from an outer scope. */
static sbIrVariable *create_upvalue(sbIrChunk *ck, sbIrVariable *v, usize slot_id) {
  sbIrVariable *new_var = sbArena_alloc(&ck->program->arena, sizeof(sbIrVariable));
  new_var->is_upvalue = TRUE;

  /* Upvalues have their own series of sequential IDs distinct from normal variables. */
  new_var->slot_id = slot_id;

  /* We can close over a variable if it has been assigned a value somewhere in our
   * scope. */
  new_var->introduced = v->introduced;

  /* Upvalues actually can be closed over, but when initially created they won't be. */
  new_var->closed_over = FALSE;

  /* This tells us where in the sequence of nested scopes this variable exists. */
  new_var->mapping_index = v->mapping_index;

  return new_var;
}

/* inside a function, when we find a variable from outer scope, we need to (A) mark the
 * variable to say it needs to be stored on the heap / indirectly in a reference, and
 * (B) we need to write down that our *function* closes over this variable so that at
 * the place where the function is constructed we can tell the program to save the function
 * value with that variable's value */
static sbIrVariable *register_upvalue(hIrChunk ck, usize variable_index) {
  varmapentry *e = &BUFFER_INDEX(ck->program->varmapping, varmapentry, variable_index);
  e->var->closed_over = TRUE;

  sbIrVariable *existing_upvalue = NULL;
  BUFFER_ITER(ck->closed_vars, sbIrVariable*, var) {
    if ((*var)->mapping_index == variable_index) {
      existing_upvalue = *var;
      break;
    }
  }

  if (!existing_upvalue) {
    /* Record that we close over this index so that the outer function knows to record
     * it when creating our closure. (It might, itself, need to close over stuff as
     * well...) */
    sbIrVariable *upvalue_var = create_upvalue(ck, e->var, ck->num_upvalues);
    sbBuffer_append(&ck->closed_vars, &upvalue_var, sizeof(sbIrVariable*));
    ck->num_upvalues ++;
    return upvalue_var;
  } else {
    return existing_upvalue;
  }
}

/* When we have compiled a chunk and are inserting its ID into the code, we need
 * to mark which variables are being bound into its closure -- ok, fine, but we
 * also need to check if those variables are external *to us* also so that we can
 * know whether we need to pull them from *our* upvalues or our normal variables
 * (and also so that our outer function knows to close over them for us when it
 * is creating us, even if we ourselves don't actually use them) */
static sbIrVariable *bind_upvalue(hIrChunk ck, usize variable_index) {
  if (variable_index >= ck->lowest_var_id) {
    /* inner function wants one of our normal variables */
    return BUFFER_INDEX(ck->program->varmapping, varmapentry, variable_index).var;
  } else {
    /* need to close over this from *our* outer scope as well... */
    return register_upvalue(ck, variable_index);
  }
}

/* to look up a variable name in source code */
static sbIrVariable *var_name(hIrChunk ck, const char *name, usize name_len) {
  usize index;
  sbIrVariable *v = existing_var(ck, name, name_len, &index);

  if (v == NO_VAR) {
    /* TODO do a different thing here */
    chunk_error(ck, "unknown variable name! '%s'\n", name);
  }

  if (index >= ck->lowest_var_id) {
    /* normal variable in inner function scope */
    return v;
  } else {
    /* closed over variable from outer scope */
    return register_upvalue(ck, index);
  }
}

/* we can introduce new variables into a scope using LET */
static sbIrVariable *create_var(hIrChunk ck, const char *name, usize name_len) {
  sbIrVariable *new_var = sbArena_alloc(&ck->program->arena, sizeof(sbIrVariable));

  sbIrVariable *already_existing = existing_var(ck, name, name_len, NULL);
  if (already_existing != NO_VAR) {
    // TODO this should probably just be a warning
    chunk_error(ck, "redeclaration of variable '%s'!\n", name);
    return NO_VAR;
  }

  usize nvars = ck->program->varmapping.size / sizeof(varmapentry);

  *new_var = (sbIrVariable) {0};
  new_var->slot_id = nvars - ck->lowest_var_id;
  new_var->mapping_index = nvars;
  if (new_var->slot_id + 1 > ck->variable_count) {
    /* store max variable slot count used so we know how many
     * need to be allocated for our chunk */
    ck->variable_count = new_var->slot_id + 1;
  }

  sbBuffer_append(&ck->program->varmapping, &(varmapentry) {
    .name = name,
    .name_len = name_len,
    .var = new_var,
  }, sizeof(varmapentry));

  return new_var;
}

static sbIrLabel *new_label(hIrChunk ck) {
  sbIrLabel *l = sbArena_alloc(&ck->program->arena, sizeof(sbIrLabel));
  ck->label_count ++;
  l->id = ck->label_count;
  return l;
}

static sbIrBindList *new_bind_list(hIrChunk ck, sbIrExpr *value, usize pre_splat_count) {
  sbIrBindList bl = {
    .this = value,
    .pre_splat_count = pre_splat_count,
  };
  sbIrBindList *where_to_put = sbArena_alloc(&ck->program->arena, sizeof(sbIrBindList));
  memcpy(where_to_put, &bl, sizeof(sbIrBindList));
  return where_to_put;
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
  /* when creating a 'literal' of a function 'func' inside another chunk 'ck',
   * 'func' tells us which variables it closes over from the outer scope. we need
   * to convert these into references to variables in ck's scope so that it knows
   * which of its variables to save for this particular function. (it also knows
   * to heap-allocate those variables as sbRef because their closed_over flag is
   * set, but if we have multiple functions closing over different variables we
   * need to know which is which, and also these closed variables might be upvalues
   * to ck as well. */
  sbIrExpr *e = new_expr(ck, &(sbIrExpr) {
    .type = IR_E_FUNC,
    .func.chunk = func,
  });

  init_buffer(ck->program, &e->func.bound, 256);
  BUFFER_ITER(func->closed_vars, sbIrVariable*, var) {
    sbIrVariable *outer_ref = bind_upvalue(ck, (*var)->mapping_index);
    sbBuffer_append(&e->func.bound, &outer_ref, sizeof(sbIrVariable*));
  }

  return e;
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

static sbIrExpr *expr_send(hIrChunk ck, sbIrExpr *target, sbIrExpr *message) {
  return new_expr(ck, &(sbIrExpr) {
    .type = IR_E_SEND,
    .send.target = target,
    .send.message = message,
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

static void put_assign(hIrChunk ck, sbIrVariable *var, sbIrExpr *expr) {
  put_ir_stmt(ck, &(sbIrStmt) {
    .type = IR_S_ASSIGN,
    .assign.var = var,
    .assign.expr = expr,
  });
}

static void put_bind(hIrChunk ck, sbIrBindList *vars, sbIrExpr *values) {
  put_ir_stmt(ck, &(sbIrStmt) {
    .type = IR_S_BIND,
    .bind.vars = vars,
    .bind.values = values,
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
static sbIrExpr *compile_ast_expr(hIrChunk ck, sbAst exprast, flag list_context);
static sbIrVariable *compile_ast_var(hIrChunk ck, sbAst node);
static sbIrBindList *compile_ast_bind_list(hIrChunk ck, sbAst node, flag create_vars, sbIrNameIntroduceType type);
static sbIrExpr *compile_ast_list(hIrChunk ck, sbAst node);

static sbIrVariable *lookup_node_var(hIrChunk ck, sbAst node) {
  if (node->type != AST_NODE_NAME) {
    PANIC("wrong node type passed to lookup_node_var (%d)", node->type);
  }
  const char *vname = sbSymbol_name(node->symb);
  /* TODO I don't love strlen here. Is there something better we can do to
   * handle symbols in general? */
  return var_name(ck, vname, strlen(vname));
}

static sbIrVariable *create_node_var(hIrChunk ck, sbAst node) {
  if (node->type != AST_NODE_NAME) {
    PANIC("wrong node type passed to create_node_var (%d)", node->type);
  }
  const char *vname = sbSymbol_name(node->symb);
  /* TODO I don't love strlen here. Is there something better we can do to
   * handle symbols in general? */
  return create_var(ck, vname, strlen(vname));
}

static sbIrChunk *compile_ast_function(hIrProgram ir, sbAst paramsAst, sbAst seqast) {
  sbIrChunk *ck = new_chunk(ir);

  /* create new scope to hold function parameters */
  usize parameter_scope_level = ck->program->varmapping.size / sizeof(varmapentry);

  /* compile parameters */
  sbIrBindList *arg_binding = compile_ast_bind_list(ck, paramsAst, TRUE, BY_PARAM);
  if (arg_binding) {
    put_bind(ck, arg_binding, NULL);
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
    sbAst node = considering->seq.left;
    /* make sure all DEF names are available through the whole scope, so code can
     * call functions defined later in the program. however, LET won't receive a value / be
     * allowed to be used until the actual line of the declaration */
    if (node->type == AST_NODE_DEF) {
      /* if something was declared with DEF, it must be introduced, because DEF requires
       * you to provide a value. so we can close over DEF's that are defined later in the
       * scope. */
      sbIrVariable *V1 = create_node_var(ck, node->seq.left);
      V1->introduced = BY_DEF;
    }
    considering = considering->seq.right;
  }

  considering = seqast;
  while (considering != NO_NODE) {
    sbAst node = considering->seq.left;
    /* now, we actually go through and compile all the functions with assignments, so that
     * they will be available to call through the whole scope. */
    if (node->type == AST_NODE_DEF) {
      if (node->seq.left->type != AST_NODE_NAME) PANIC("expected name for function in def!");
      if (node->seq.right->type != AST_VAL_FUNC) PANIC("expected function body for function in def!");
      sbAst func_node = node->seq.right;
      sbAst params = func_node->seq.left;
      sbAst body = func_node->seq.right;
      if (params != NO_NODE && params->type != AST_NODE_MULTIVAL) {
        PANIC("expected multival as function params!");
      }
      if (body->type != AST_NODE_SEQ) PANIC("expected SEQ node as function body!");

      sbIrVariable *V1 = lookup_node_var(ck, node->seq.left);
      sbIrChunk *C1 = compile_ast_function(ck->program, params, body);
      sbIrExpr *E1 = expr_func(ck, C1);
      put_assign(ck, V1, E1);
    }

    if (node->type == AST_NODE_LET) {
      sbAst N1 = node->seq.left;  /* things to bind to */
      compile_ast_bind_list(ck, N1, TRUE, BY_LET);
    }
    considering = considering->seq.right;
  }

  /* now reset the 'introduced' flag of anything that was introduced by LET so
   * that we can't refer to them earlier than they are declared */
  BUFFER_ITER_FROM(ck->program->varmapping, varmapentry, entry, ck->lowest_var_id) {
    if (entry->var->introduced == BY_LET) {
      entry->var->introduced = NOT_INTRODUCED;
    }
  }

  considering = seqast;
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
        E1 = compile_ast_expr(ck, node->seq.left->seq.left, FALSE);
      }
      put_return(ck, E1);
      break;

    case AST_NODE_LET:
      /* The LET part actually gets moved to the top of the enclosing block along with
       * any DEFs. So we only care about encountering a LET node in this context if it
       * is an assignment.
       *
       * Then the assignment part basically gets turned into the equivalent of something
       * like
       *        ...[a, b, c] = ...[1, 2, 3]
       *
       * We destructure some multi-value that we're assigning to onto the stack along
       * with a count, and then we use the argument-binding instructions to assign
       * those pieces to variables.  */
      if (node->seq.right != NO_NODE) {
        if (node->seq.left->type != AST_NODE_MULTIVAL) PANIC("expected multival on left side of let!");
        if (node->seq.right->type != AST_NODE_MULTIVAL) PANIC("expected multival on right side of let!");
        sbIrBindList *variables = compile_ast_bind_list(ck, node->seq.left, FALSE, BY_LET); /* things being assigned to */
        sbIrExpr *assigned = compile_ast_list(ck, node->seq.right); /* values to assign */
        put_bind(ck, variables, assigned);
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
        E1 = compile_ast_expr(ck, N2->seq.left, TRUE);
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
      /* DEF should already have been compiled at the beginning of the block. */
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
      E1 = compile_ast_expr(ck, node->seq.left, FALSE);
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
        E1 = compile_ast_expr(ck, node->tri.left, FALSE);
        L1 = new_label(ck);
        put_jump_if_no(ck, E1, L1);
        compile_ast_stmtseq(ck, node->tri.center, implicit_return);
        put_label(ck, L1);
      } else {
        /* yes else branch */
        E1 = compile_ast_expr(ck, node->tri.left, FALSE);
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
        E1 = compile_ast_expr(ck, node->seq.right, FALSE);
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
      put_expr(ck, compile_ast_expr(ck, node, FALSE));
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
    *place_here = expr_list(ck, compile_ast_expr(ck, considering->seq.left, TRUE));
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
    sbIrExpr *compiled_entry = expr_list(ck, compile_ast_expr(ck, hash_entry->seq.left, FALSE)); // key
    compiled_entry->list.next = compile_ast_expr(ck, hash_entry->seq.right, FALSE); // value

    *place_here = expr_hash(ck, compiled_entry);
    place_here = &(*place_here)->list.next;
    considering = considering->seq.right;
  }
  return list;
}

/* compile one individual element to bind to: might be a bare variable name,
 * or might be "...name", or might be some other expression, in which case
 * we bind by matching it exactly? (TODO) */
static sbIrExpr *compile_ast_binding(hIrChunk ck, sbAst node, flag should_create_var, sbIrNameIntroduceType type) {
  if (node->type == AST_NODE_NAME) {
    sbIrVariable *V1;
    if (should_create_var) {
      V1 = create_node_var(ck, node);
    } else {
      V1 = lookup_node_var(ck, node);
    }
    V1->introduced = type;
    return expr_var(ck, V1);
  } else if (node->type == AST_NODE_OP) {
    sbIrExpr *left = NULL, *right = NULL;
    if (node->op.left != NO_NODE) {
      left = compile_ast_binding(ck, node->op.left, should_create_var, type);
    }
    if (node->op.right != NO_NODE) {
      right = compile_ast_binding(ck, node->op.right, should_create_var, type);
    }
    return expr_op(ck, node->op.type, left, right);
  } else {
    /* assume it's a literal value (TODO allow destructuring of list and
     * hash via this) */
    return compile_ast_expr(ck, node, FALSE);
  }
}

/* compile a list of names to bind to: on the left side of a 'let' expression,
 * for function parameters, and at the top of a 'match' expression. */
static sbIrBindList *compile_ast_bind_list(hIrChunk ck, sbAst node, flag create_vars, sbIrNameIntroduceType type) {
  sbAst considering = node;
  sbIrBindList *list = NULL;
  flag was_splat = FALSE;
  usize pre_splat_count = 0;
  while (considering != NO_NODE) {
    /* bind list needs to be reversed, because we're going to bind to some stack
     * that was built bottom-to-top. so we build it in the reverse order from a
     * value list. */
    sbAst elem = considering->seq.left;
    if (elem->type == AST_NODE_OP && elem->op.type == AST_OP_SPLAT) {
      if (!was_splat) {
        was_splat = TRUE;
      } else {
        chunk_error(ck, "Multiple '...' assignments are not permitted!\n");
        return NULL;
      }
    } else if (!was_splat) {
      /* keep track of how many variables existed *before* we ran into a "...a"
       * type expression, so we know how many to leave on the stack when binding
       * a "..." */
      pre_splat_count ++;
    }
    sbIrBindList *new_list = new_bind_list(
        ck, compile_ast_binding(ck, considering->seq.left, create_vars, type), pre_splat_count
    );
    new_list->next = list;
    list = new_list;
    considering = considering->seq.right;
  }

  return list;
}

static sbIrExpr *compile_ast_expr(hIrChunk ck, sbAst node, flag list_context) {
  if (node == NO_NODE) return NULL;

  if (node->type == AST_NODE_FUNCCALL) {
    sbIrExpr *called = compile_ast_expr(ck, node->seq.left, FALSE);
    sbIrExpr *params = compile_ast_list(ck, node->seq.right);
    return expr_call(ck, called, params);
  } else if (node->type == AST_NODE_METHODCALL) {
    sbIrExpr *target = compile_ast_expr(ck, node->seq.left, FALSE);
    sbIrExpr *message = compile_ast_list(ck, node->seq.right);
    return expr_send(ck, target, message);
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
    if (node->op.type == AST_OP_SPLAT) {
      if (!list_context) {
        chunk_error(ck, "'...' not allowed outside a list");
        return NULL;
      } else {
        return expr_op(ck, AST_OP_SPLAT, compile_ast_expr(ck, node->op.left, FALSE), NULL);
      }
    } else {
      sbIrExpr *left = NULL, *right = NULL;
      if (node->op.left != NO_NODE) {
        left = compile_ast_expr(ck, node->op.left, FALSE);
      }
      if (node->op.right != NO_NODE) {
        right = compile_ast_expr(ck, node->op.right, FALSE);
      }
      return expr_op(ck, node->op.type, left, right);
    }
  } else if (node->type == AST_VAL_INT) {
    return expr_value(ck, &HVINT(node->i));
  } else if (node->type == AST_VAL_STRING) {
    return expr_value(ck, &HVSTR(node->str));
  } else if (node->type == AST_VAL_SYMBOL) {
    return expr_value(ck, &HVSYM(node->symb));
  } else if (node->type == AST_VAL_NIL) {
    return expr_value(ck, &HVNIL);
  } else if (node->type == AST_NODE_NAME) {
    /* TODO: I don't want to use strlen. Can we remember the lengths of symbols? */
    sbIrVariable *var = compile_ast_var(ck, node);
    if (var->introduced) {
      return expr_var(ck, var);
    } else {
      chunk_error(ck, "Variable '%s' has not been declared here!\n", sbSymbol_name(node->symb));
      return NULL;
    }
  } else {
    PANIC("expr todo! (%d)", node->type);
  }
}
