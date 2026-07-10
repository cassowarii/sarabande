#include "compile/ir.h"

#include "data/symbol.h"

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
        debug("\b\b");
      }
      debug(" }");
      break;
    case IR_E_VALUE:
      if (e->value.type == IT_NIL) {
        debug("nil");
      } else if (e->value.type == IT_INTEGER) {
        debug("%lld", (long long)e->value.integer);
      } else if (e->value.type == IT_SYMBOL) {
        debug(":%s", sbSymbol_name(e->value.symbol));
      } else {
        debug("some value");
      }
      break;
    case IR_E_OP:
      debug("(");
      print_expr(e->op.left);
      debug(" %c ", e->op.type);
      if (e->op.right) {
        print_expr(e->op.right);
      }
      debug(")");
      break;
    case IR_E_CALL:
      debug("CALL: ");
      print_expr(e->call.func);
      debug(" with params (");
      if (e->call.param) {
        print_expr(e->call.param);
      }
      debug(")");
      break;
    case IR_E_SEND:
      debug("SEND: ");
      print_expr(e->send.target);
      debug(" <~( ");
      print_expr(e->send.message);
      debug(" )");
      break;
    case IR_E_LIST:
      if (e->list.next) {
        print_expr(e->list.next);
        if (e->list.next->list.next) {
          debug(", ");
        }
      }
      if (e->list.this) {
        print_expr(e->list.this);
      }
      break;
    case IR_E_CONTEXT:
      debug("(context var %s)", sbSymbol_name(e->symbol));
      break;
    default:
      debug("something %d", e->type);
  }
}

static void print_bind_list(sbIrBindList *e) {
  if (e->next) {
    /* print in reverse order */
    print_bind_list(e->next);
    debug(", ");
  }
  print_expr(e->this);
}

static void print_stmt(sbIrStmt *s) {
  switch (s->type) {
    case IR_S_BIND:
      debug("  bind [");
      print_bind_list(s->bind.vars);
      debug("] to [");
      if (s->bind.values) {
        print_expr(s->bind.values);
      } else {
        debug(" ~~~ ");
      }
      debug("]\n");
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
        debug("NOTHING!! (PROBABLY A BUG)");
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
