#include "compile/ir.h"

#include "parse/ast.h"
#include "data/symbol.h"

#include "mem/debug.h"

#define IF_NOT 1
#define IF_YES 0
#define ALWAYS 0

typedef struct varmapentry {
  const char *name;
  usize name_len;
  sbIrVariable *var;
} varmapentry;

static void compile_ast_stmtseq(hIrChunk ck, sbAst seqast);
static sbIrChunk *new_chunk(hIrProgram ir);
static void chunk_deinitialize(hIrChunk ck);
static void print_stmt(sbIrStmt *s);

void sbIrProgram_initialize(hIrProgram ir, usize initial_arena_size) {
  *ir = (sbIrProgram) {0};
  sbArena_initialize(&ir->arena, initial_arena_size);
  sbBuffer_initialize(&ir->chunks, 4096);
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
  sbIrChunk *ck = new_chunk(ir);
  compile_ast_stmtseq(ck, ast);
}

void sbIrProgram_print(hIrProgram ir) {
  usize nchunks = ir->chunks.size / sizeof(sbIrChunk*);

  for (usize i = 0; i < nchunks; i++) {
    sbIrChunk *ck = ((sbIrChunk**)ir->chunks.data)[i];
    printf("\nCHUNK %zu (%p)\n", ck->id, ck);

    usize nstmts = ck->stmts.size / sizeof(sbIrStmt*);
    for (usize j = 0; j < nstmts; j++) {
      sbIrStmt *s = ((sbIrStmt**)ck->stmts.data)[j];
      print_stmt(s);
    }

    printf("\n");
  }
}

/* --- */

static sbIrChunk *new_chunk(hIrProgram ir) {
  usize nchunks = ir->chunks.size / sizeof(sbIrChunk*);
  sbIrChunk *chunk = sbArena_alloc(&ir->arena, sizeof(sbIrChunk));
  sbBuffer_initialize(&chunk->stmts, 1024);
  chunk->id = nchunks;
  chunk->program = ir;
  sbBuffer_append(&ir->chunks, &chunk, sizeof(sbIrChunk*));
  return chunk;
}

static void chunk_deinitialize(hIrChunk ck) {
  sbBuffer_deinitialize(&ck->stmts);
}

/* we can introduce new variables into a scope using LET */
static sbIrVariable *create_var(hIrChunk ck, const char *name, usize name_len) {
  sbIrVariable *new_var = sbArena_alloc(&ck->program->arena, sizeof(sbIrVariable));

  ck->variable_count ++;
  *new_var = (sbIrVariable) {0};
  new_var->created_index = ck->variable_count,

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
  for (usize i = 0; i < nvars; i++) {
    varmapentry *entry = &((varmapentry*)ck->program->varmapping.data)[i];
    if (sbstrncmp(name, entry->name, name_len) == 0) {
      return entry->var;
    }
  }

  return NULL;
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

static sbIrExpr *expr_var(hIrChunk ck, const char *name, usize name_len) {
  sbIrVariable *var_by_name = var_name(ck, name, name_len);
  /* TODO do a different thing */
  if (!var_by_name) {
    fprintf(stderr, "unknown variable name! '%s'\n", name);
    return NULL;
  }
  return new_expr(ck, &(sbIrExpr) {
    .type = IR_E_VAR,
    .var = var_by_name,
  });
}

static sbIrExpr *expr_value(hIrChunk ck, hV *value) {
  return new_expr(ck, &(sbIrExpr) {
    .type = IR_E_VALUE,
    .value = *value,
  });
}

static sbIrExpr *expr_op(hIrChunk ck, sbAstOp op, sbIrExpr *left, sbIrExpr *right) {
  return new_expr(ck, &(sbIrExpr) {
    .type = IR_E_OP,
    .op.type = op,
    .op.left = left,
    .op.right = right,
  });
}

static void put_ir_stmt(hIrChunk ck, sbIrStmt *stmt) {
  sbIrStmt *where_to_put = sbArena_alloc(&ck->program->arena, sizeof(sbIrStmt));
  memcpy(where_to_put, stmt, sizeof(sbIrStmt));
  sbBuffer_append(&ck->stmts, &where_to_put, sizeof(sbIrStmt*));
}

static void put_label(hIrChunk ck, sbIrLabel *l) {
  put_ir_stmt(ck, &(sbIrStmt) {
    .type = IR_S_LABEL,
    .label = l,
  });
}

static void put_jump(hIrChunk ck, flag inverted, sbIrExpr *condition, sbIrLabel *label) {
  put_ir_stmt(ck, &(sbIrStmt) {
    .type = IR_S_JUMP,
    .jump.inverted = inverted,
    .jump.condition = condition,
    .jump.label = label,
  });
}

static void put_expr(hIrChunk ck, sbIrExpr *expr) {
  put_ir_stmt(ck, &(sbIrStmt) {
    .type = IR_S_EXPR,
    .expr = expr,
  });
}

static void put_assign(hIrChunk ck, sbIrVariable *assign_to, sbIrExpr *expr) {
  put_ir_stmt(ck, &(sbIrStmt) {
    .type = IR_S_ASSIGN,
    .assign.var = assign_to,
    .assign.expr = expr,
  });
}

static void put_return(hIrChunk ck) {
  put_ir_stmt(ck, &(sbIrStmt) {
    .type = IR_S_RETURN,
  });
}

static void compile_ast_stmt(hIrChunk ck, sbAst stmtast);
static sbIrExpr *compile_ast_expr(hIrChunk ck, sbAst exprast);

static void compile_ast_stmtseq(hIrChunk ck, sbAst seqast) {
  usize varmapsize = ck->program->varmapping.size;

  sbAst considering = seqast;
  while (considering != NO_NODE) {
    /* walk down the tree a line at a time */
    if (considering->type != AST_NODE_SEQ) {
      PANIC("compile_ast_stmtseq expects a SEQ node! (received instead #%d)", seqast->type);
    }

    compile_ast_stmt(ck, considering->seq.left);
    considering = considering->seq.right;
  }

  /* after the block is done, we need to reset the varmapping to not include
   * any variables that were introduced inside this block */
  sbBuffer_set_size(&ck->program->varmapping, varmapsize);
}

void compile_ast_stmt(hIrChunk ck, sbAst node) {
  sbIrExpr *E1;//, *E2;
  sbIrLabel *L1, *L2;
  switch (node->type) {
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
      L1 = new_label(ck);
      L2 = new_label(ck);
      E1 = compile_ast_expr(ck, node->seq.left);
      put_jump(ck, ALWAYS, NULL, L2);
      put_label(ck, L1);
      compile_ast_stmtseq(ck, node->seq.right);
      put_label(ck, L2);
      put_jump(ck, IF_YES, E1, L1);
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
        put_jump(ck, IF_NOT, E1, L1);
        compile_ast_stmtseq(ck, node->tri.center);
        put_label(ck, L1);
      } else {
        /* yes else branch */
        E1 = compile_ast_expr(ck, node->tri.left);
        L1 = new_label(ck);
        L2 = new_label(ck);
        put_jump(ck, IF_NOT, E1, L1);
        compile_ast_stmtseq(ck, node->tri.center);
        put_jump(ck, ALWAYS, NULL, L2);
        put_label(ck, L1);
        compile_ast_stmtseq(ck, node->tri.right);
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
        compile_ast_stmtseq(ck, node->seq.left);
        put_jump(ck, IF_YES, E1, L1);
      } else {
        put_label(ck, L1);
        compile_ast_stmtseq(ck, node->seq.left);
        put_jump(ck, ALWAYS, NULL, L1);
      }
      break;

    default:
      /* probably an expr node */
      put_expr(ck, compile_ast_expr(ck, node));
  }
}

static sbIrExpr *compile_ast_expr(hIrChunk ck, sbAst node) {
  if (node == NO_NODE) return NULL;

  if (node->type == AST_NODE_OP) {
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
  } else if (node->type == AST_NODE_NAME) {
    /* TODO: I don't want to use strlen. Can we remember the lengths of symbols? */
    return expr_var(ck, sbSymbol_name(node->symb), strlen(sbSymbol_name(node->symb)));
  } else {
    PANIC("todo!");
  }
}

static void print_expr(sbIrExpr *e) {
  printf("[ ");
  switch(e->type) {
    case IR_E_VAR:
      printf("variable %zu", e->var->created_index);
      break;
    case IR_E_VALUE:
      if (e->value.type == IT_INTEGER) {
        printf("%lld", e->value.integer);
      } else {
        printf("(some value)");
      }
      break;
    case IR_E_OP:
      print_expr(e->op.left);
      printf(" %c ", e->op.type);
      if (e->op.right) {
        print_expr(e->op.right);
      }
      break;
    default:
      printf("(something)");
  }
  printf(" ]");
}

static void print_stmt(sbIrStmt *s) {
  switch (s->type) {
    case IR_S_LABEL:
      printf("label %zu:\n", s->label->id);
      break;
    case IR_S_JUMP:
      printf("jump to label %zu", s->jump.label->id);
      if (s->jump.condition) {
        if (s->jump.inverted) {
          printf(" if not ");
        } else {
          printf(" if ");
        }
        print_expr(s->jump.condition);
      }
      printf("\n");
      break;
    case IR_S_RETURN:
      printf("return\n");
      break;
    case IR_S_EXPR:
      print_expr(s->expr);
      printf("\n");
      break;
    case IR_S_ASSIGN:
    default:
      printf("do something\n");
  }
}
