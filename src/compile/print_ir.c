#include "compile/ir.h"

static void print_stmt(sbIrStmt *s);

void sbIrProgram_print(hIrProgram ir) {
  usize nchunks = ir->chunks.size / sizeof(sbIrChunk*);

  for (usize i = 0; i < nchunks; i++) {
    sbIrChunk *ck = ((sbIrChunk**)ir->chunks.data)[i];
    debug("\nCHUNK %d with %d variables", ck->id, ck->variable_count);
    if (ck->num_upvalues > 0) {
      debug(" and %d upvalues", ck->num_upvalues);
    }
    debug("\n");

    usize nstmts = ck->stmts.size / sizeof(sbIrStmt*);
    for (usize j = 0; j < nstmts; j++) {
      sbIrStmt *s = ((sbIrStmt**)ck->stmts.data)[j];
      print_stmt(s);
    }

    debug("\n");
  }
}

/* --- */

static void print_var(sbIrVariable *v) {
  if (v->closed_over) {
    debug("special ");
  }

  if (v->is_upvalue) {
    debug("upvalue %zu", v->slot_id);
  } else {
    debug("variable %zu", v->slot_id);
  }
}

static void print_expr(sbIrExpr *e) {
  debug("(");
  switch(e->type) {
    case IR_E_VAR:
      print_var(e->var);
      break;
    case IR_E_FUNC:
      debug("{ chunk %d ", e->func.chunk->id);
      if (e->func.bound.size > 0) {
        debug("closing over ");
        BUFFER_ITER(e->func.bound, sbIrVariable*, var) {
          print_var(*var);
          debug(", ");
        }
      }
      debug("}");
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
      debug("  bind function argument to ");
      print_var(s->arg.var);
      debug("\n");
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
      debug("  ");
      print_var(s->assign.var);
      debug(" = ");
      print_expr(s->assign.expr);
      debug("\n");
      break;
    default:
      debug("  (do something)\n");
  }
}
