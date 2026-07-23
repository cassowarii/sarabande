#include "compile/emit.h"

#define STRING1(...) #__VA_ARGS__
#define STRING2(...) STRING1(__VA_ARGS__)
#define EMIT(x) (emit(cm, x))
#define EARG(x) (emit_arg(cm, x))

struct labelpos {
  sbIrLabel *label;
  u32 offset;
};

void compile_chunk(sbVmCompiler *cm, sbIrChunk *chunk);
void compile_stmt(sbVmCompiler *cm, sbIrStmt *stmt);
void compile_expr(sbVmCompiler *cm, sbIrExpr *expr);
void compile_ref(sbVmCompiler *cm, sbIrExpr *expr, flag is_lref);
void compile_maybe_ref(sbVmCompiler *cm, sbIrExpr *expr, flag is_lref, sbOpcode op, sbOpcode op_ind);
void compile_op(sbVmCompiler *cm, sbAstOp op);

void sbEmit_compile_program(sbVmProgram *vp, sbIrProgram *ir, flag debugmode) {
  sbVmCompiler cm = sbVmCompiler_create(4096, 4096, debugmode);

  int nchunks = ir->chunks.size / sizeof(sbIrChunk*);
  for (int i = 0; i < nchunks; i++) {
    sbIrChunk *chunk = ((sbIrChunk**)ir->chunks.data)[i];

    compile_chunk(&cm, chunk);

    sbVmProgram_add_block(vp, &cm);
    sbVmCompiler_reset(&cm);
  }

  sbVmCompiler_deinitialize(&cm);
}

/* --- */

void emit(sbVmCompiler *cm, sbOpcode opcode) {
  if (cm->debugmode) debug("%s\n", g_opcode_names[opcode]);
  sbVmCompiler_write_code(cm, (u8[]) { opcode }, 1);
}

void emit_arg(sbVmCompiler *cm, i64 actual_number) {
  if (cm->debugmode) debug("    arg %lld\n", (long long)actual_number);

  u8 buf[9];
  u64 number = actual_number;
  if (0 <= actual_number && actual_number < 253) {
    buf[0] = number;
    sbVmCompiler_write_code(cm, buf, 1);
  } else if (-32768 < actual_number && actual_number < 32768) {
    buf[0] = BC_LONG_NUM;
    if (actual_number < 0) {
      number = actual_number + 65536;
    }
    buf[1] = (number >>  8) & 0xFF;
    buf[2] = (number >>  0) & 0xFF;
    sbVmCompiler_write_code(cm, buf, 3);
  } else if (-(1L << 31) < actual_number && actual_number < (1L << 31)) {
    buf[0] = BC_VLONG_NUM;
    if (actual_number < 0) {
      number = actual_number + (1LL << 32);
    }
    buf[1] = (number >> 24) & 0xFF;
    buf[2] = (number >> 16) & 0xFF;
    buf[3] = (number >>  8) & 0xFF;
    buf[4] = (number >>  0) & 0xFF;
    sbVmCompiler_write_code(cm, buf, 5);
  } else {
    buf[0] = BC_VVLONG_NUM;
    number = (u64)actual_number;
    buf[1] = (number >> 56) & 0xFF;
    buf[2] = (number >> 48) & 0xFF;
    buf[3] = (number >> 40) & 0xFF;
    buf[4] = (number >> 32) & 0xFF;
    buf[5] = (number >> 24) & 0xFF;
    buf[6] = (number >> 16) & 0xFF;
    buf[7] = (number >>  8) & 0xFF;
    buf[8] = (number >>  0) & 0xFF;
    sbVmCompiler_write_code(cm, buf, 9);
  }
}


void compile_chunk(sbVmCompiler *cm, sbIrChunk *chunk) {
  if (cm->debugmode) debug("\n--block %d--\n", chunk->id);

  int nstmts = chunk->stmts.size / sizeof(sbIrStmt*);
  if (chunk->id > 0) {
    /* don't check args for initial block
     * (note: when we have modules or whatever, this might
     * be an incorrect way to do this) */
    /* TODO re-add checking this */
    //EMIT(BC_NUMARG);
    //EARG(chunk->num_args);

    if (chunk->num_args == 0) {
      /* if no arguments, pop the 0 off the stack. TODO we need to check it's the right amount */
      EMIT(BC_POP);
    }
  }

  if (chunk->variable_count > 0) {
    EMIT(BC_ALLOC_VARS);
    EARG(chunk->variable_count);
  }

  for (int i = 0; i < nstmts; i++) {
    sbIrStmt *stmt = ((sbIrStmt**)chunk->stmts.data)[i];
    compile_stmt(cm, stmt);
  }

  /* Go back and fill in locations for forward jumps, whose
   * positions we now know. */
  usize nlabelpos = cm->label_positions.size / sizeof(struct labelpos);
  for (int i = 0; i < nlabelpos; i++) {
    struct labelpos lp = ((struct labelpos*)cm->label_positions.data)[i];
    u8 location_bytes[4];
    u32 position = lp.label->block_position;
    if (cm->debugmode) debug("now we know that the jump at %d should go to %d\n", lp.offset - 2, position);
    location_bytes[0] = (position >> 24) & 0xFF;
    location_bytes[1] = (position >> 16) & 0xFF;
    location_bytes[2] = (position >>  8) & 0xFF;
    location_bytes[3] = (position >>  0) & 0xFF;
    sbVmCompiler_overwrite_code_at(cm, lp.offset, location_bytes, 4);
  }
  sbBuffer_set_size(&cm->label_positions, 0);
}

/* when compiling one line at a time, we don't know the position
 * of labels we haven't seen yet. we leave a 4-byte slot for a blank
 * address, and say "please put the address of this label here later!" */
void record_labelpos(sbVmCompiler *cm, sbIrLabel *label, u32 offset) {
  struct labelpos lp = {
    .label = label,
    .offset = offset,
  };

  sbBuffer_append(&cm->label_positions, &lp, sizeof(struct labelpos));
}

/* things that implicitly dereference when used in a value context but
 * can be assigned to on the left side of an = */
flag is_implicit_ref(sbIrExpr *expr) {
  return expr->type == IR_E_VAR || expr->type == IR_E_DOT || (expr->type == IR_E_OP && expr->op.type == AST_OP_INDEX);
}

void compile_list(sbVmCompiler *cm, sbIrExpr *expr);
void compile_bind_list(sbVmCompiler *cm, sbIrBindList *list);
void compile_stmt(sbVmCompiler *cm, sbIrStmt *stmt) {
  if (cm->debugmode) debug("%3zu ", sbVmCompiler_get_position(cm));
  switch (stmt->type) {
    case IR_S_EXPR:
      compile_expr(cm, stmt->expr);
      EMIT(BC_POP);
      break;
    case IR_S_JUMP:
      if (stmt->jump.condition == NULL) {
        EMIT(BC_JMP);
      } else {
        compile_expr(cm, stmt->jump.condition);
        if (cm->debugmode) debug("%3zu ", sbVmCompiler_get_position(cm));
        if (stmt->jump.inverted) {
          EMIT(BC_JF);
        } else {
          EMIT(BC_JT);
        }
      }
      if (stmt->jump.label->found_yet) {
        EARG(stmt->jump.label->block_position);
      } else {
        EMIT(BC_VLONG_NUM);
        /* we have to remember to come back and fill in the address later when we
         * know where this label is! */
        record_labelpos(cm, stmt->jump.label, sbVmCompiler_get_position(cm));
        /* leave behind four zeroes at this offset that we can later put the
         * address into */
        EARG(0);
        EARG(0);
        EARG(0);
        EARG(0);
      }
      break;
    case IR_S_LABEL:
      stmt->label->found_yet = TRUE;
      stmt->label->block_position = sbVmCompiler_get_position(cm);
      if (cm->debugmode) debug("\n");
      break;
    case IR_S_ASSIGN:
      compile_expr(cm, stmt->assign.value);
      if (stmt->assign.where->type == IR_E_VAR) {
        /* direct assignment to some variable name or other */
        sbIrVariable *var = stmt->assign.where->var;
        if (var->is_reference) {
          if (!var->is_upvalue) {
            /* let &a = &b; a = 5 # <-- should set b, too! */
            EMIT(BC_LD_VAR);
          } else {
            /* same, but for upvalue */
            EMIT(BC_LD_UPVAL);
          }
          EARG(var->slot_id);
          EMIT(BC_REF_PUT);
        } else {
          if (!var->is_upvalue) {
            /* normal assignment to normal variable */
            EMIT(BC_ST_VAR);
          } else {
            /* assignment to normal upvalue variable */
            EMIT(BC_ST_UPVAL);
          }
          EARG(var->slot_id);
        }
      } else if (stmt->assign.where->type == IR_E_OP && stmt->assign.where->op.type == AST_OP_DEREF) {
        /* assignment to like *<....> = <....> */
        compile_expr(cm, stmt->assign.where->op.left);
        /* this should leave us with a pointer to expr on top of stack */
        EMIT(BC_REF_PUT);
      } else if (stmt->assign.where->type == IR_E_OP && stmt->assign.where->op.type == AST_OP_INDEX) {
        /* a[b] = whatever, a::b = whatever */
        /* there is an IMPLICIT DEREFERENCE that occurs here. op::refindex returns an LREF,
         * then we assign through the reference we receive, i.e.
         *    a[b] = c
         * really means
         *    *((&a)->.[op::index]->(b)) = c
         * The reason for this has to do with assigning through nested data structures.
         * Doing it this way just makes the whole semantics feel more natural. It's similar
         * to returning a reference from indexing in C++, I guess. */
        compile_expr(cm, stmt->assign.where->op.right);     /* index we are using */
        compile_maybe_ref(cm, stmt->assign.where->op.left, TRUE, BC_OP_INDEXLREF, BC_OP_INDEXLREF_IND);
        EMIT(BC_REF_PUT);
      } else if (stmt->assign.where->type == IR_E_DOT) {
        /* same as op::index, essentially. the dot returns a reference to the inside of the
         * thing, then we can assign to that reference */
        compile_expr(cm, stmt->assign.where->dot.param);
        EMIT(BC_LD_IMM);
        EARG(1);
        compile_maybe_ref(cm, stmt->assign.where->dot.target, TRUE, BC_DOT, BC_DOT_IND);
        EMIT(BC_REF_PUT);
      } else {
        PANIC("This type of assignment operation is not supported!");
      }
      break;
    case IR_S_BIND:
      if (stmt->bind.values) {
        compile_list(cm, stmt->bind.values);
      }
      compile_bind_list(cm, stmt->bind.vars);
      break;
    case IR_S_RETURN:
      if (stmt->expr) {
        compile_expr(cm, stmt->expr);
      }
      EMIT(BC_RET);
      break;
    default:
      PANIC("haven't implemented this yet!\n");
  }
}

/* compile... this isn't actually really for lists only, it's compile a
 * "multival": the value type of list that is a bunch of things separated
 * by commas, that, e.g. things can be splatted into. this also handles
 * function arguments and such. leaves its elements bottom-to-top on the
 * stack, with the top of the stack being the number of elements. this
 * can then be passed to a function using BC_CALL, or converted to a list
 * using BC_LIST_GATHER, or destructured as arguments using BC_ST_ARG and
 * friends. */
void compile_list(sbVmCompiler *cm, sbIrExpr *expr) {
  sbIrExpr *considering = expr;
  usize count = 0;
  flag was_splat = FALSE;
  while (considering != IR_EMPTY_LIST) {
    sbIrExpr *elem = considering->list.this;
    if (elem->type == IR_E_OP && elem->op.type == AST_OP_SPLAT) {
      if (!was_splat) {
        /* first splatted thing in list: emit non-splat argument count,
         * then spill list onto it (LIST_SPILL takes a list and a number,
         * and unpacks the list onto the stack, then adds the length of
         * the list to the number) */
        was_splat = TRUE;
        EMIT(BC_LD_IMM);
        EARG(count);
      }
      /* compile whatever's behind the splat, and splat it */
      compile_expr(cm, elem->op.left);
      EMIT(BC_LIST_SPILL);
    } else if (was_splat) {
      /* if was splat (but not this one), we can no longer rely on the
       * number of commas in the list to say how many arguments (or
       * whatever) we are passing. so we have to start counting one
       * by one */
      compile_expr(cm, elem);
      /* swap to put the number on top, then increment */
      EMIT(BC_SWAP);
      EMIT(BC_OP_INCR);
    } else {
      /* blissfully unaware of the splat */
      compile_expr(cm, elem);
    }
    count ++;
    considering = considering->list.next;
  }

  if (!was_splat) {
    /* if no splat, just put the total count here. if there was a splat,
     * we counted them already manually */
    EMIT(BC_LD_IMM);
    EARG(count);
  }
}

void compile_hash(sbVmCompiler *cm, sbIrExpr *expr) {
  sbIrExpr *considering = expr;
  usize count = 0;
  while (considering) {
    count ++;
    /* alternate keys and values on stack. push keys first,
     * then values */
    compile_expr(cm, considering->list.this->list.this);
    compile_expr(cm, considering->list.this->list.next);
    considering = considering->list.next;
  }
  EMIT(BC_LD_IMM);
  EARG(count); /* calling convention: store argument count on stack */
}

/* destructuring assignment, basically. take the output of compile_list,
 * and assign it to various variables (or (TODO) fail in some mysterious
 * way (exception?) if there is the wrong amount of stuff) */
void compile_bind_list(sbVmCompiler *cm, sbIrBindList *list) {
  for (sbIrBindList *considering = list; considering; considering = considering->next) {
    sbIrExpr *elem = considering->this;
    if (elem->type == IR_E_OP) {
      if (elem->op.type == AST_OP_SPLAT) {
        /* okay. we want to leave some number of elements on the stack
         * for whatever other arguments there are. currently we have the
         * total count on top of the stack. so, we'll reduce the count
         * by the number we want to save, gather into a list, and assign
         * to that, then replace the count we wanted to keep on the stack */
        if (list->pre_splat_count > 0) {
          EMIT(BC_LD_IMM);
          EARG(list->pre_splat_count);
          EMIT(BC_OP_SUB);
        }
        /* create list of this length and store in thing */
        EMIT(BC_LIST_GATHER);
        /* TODO actually, vv THIS vv should be a recursive call. otherwise,
         * we don't actually check that it's a variable that the "..." is
         * attached to, and we may fail in weird cases like "...2". but we
         * need to restructure the BindList data structure. */
        EMIT(BC_ST_VAR);
        EARG(elem->op.left->var->slot_id);
        if (list->pre_splat_count > 0) {
          /* put pre splat count back if we need it, to bind the rest of
           * the variables */
          EMIT(BC_LD_IMM);
          EARG(list->pre_splat_count);
        }
      } else if (elem->op.type == AST_OP_REF) {
        /* this should also probably be some kind of recursive thing, to handle
         * situations like let &&a = whatever. (if that's even doable...?)
         * but right now we just only permit & before variable names on the
         * left side of a let */
        /* storing here is the same as normal, because the actual variable
         * slot saves the reference. however, assignment etc. is statically
         * known to work differently so we will adjust those */
        EMIT(BC_ST_ARG);
        EARG(elem->op.left->var->slot_id);
      }
    } else if (elem->type == IR_E_VAR) {
      /* normal sequence, no splat (so far): top thing goes in this
       * variable using ST_ARG */
      EMIT(BC_ST_ARG);
      EARG(elem->var->slot_id);
    } else {
      PANIC("bind todo!");
    }
  }
}

void compile_expr(sbVmCompiler *cm, sbIrExpr *expr) {
  switch(expr->type) {
    case IR_E_OP:
      if (expr->op.type == AST_OP_REF) {
        /* in expr context, the & operator generally prevents 'collapsing'
         * of references to values when applied to a[b] or a.b or a::b */
        compile_ref(cm, expr->op.left, FALSE);
      } else {
        compile_expr(cm, expr->op.left);
        if (expr->op.right) {
          compile_expr(cm, expr->op.right);
        }
        compile_op(cm, expr->op.type);
      }
      break;
    case IR_E_CALL:
      /* calling convention: store argument count on stack */
      compile_list(cm, expr->call.param);
      compile_maybe_ref(cm, expr->call.func, TRUE, BC_CALL, BC_CALL_IND);
      break;
    case IR_E_DOT:
      compile_expr(cm, expr->dot.param);
      /* TODO dot should always take 1 parameter, so this is not needed */
      EMIT(BC_LD_IMM);
      EARG(1);
      compile_expr(cm, expr->dot.target);
      EMIT(BC_DOT);
      EMIT(BC_OP_DEREF);
      break;
    case IR_E_CONTEXT:
      EMIT(BC_LD_CTX);
      EARG(expr->symbol);
      break;
    case IR_E_VAR:
      if (expr->var->is_upvalue) {
        EMIT(BC_LD_UPVAL);
      } else {
        EMIT(BC_LD_VAR);
      }
      EARG(expr->var->slot_id);

      /* when a variable that is_reference is referenced
       * in non-assignment context, it automatically dereferences */
      if (expr->var->is_reference) {
        EMIT(BC_OP_DEREF);
      }
      break;
    case IR_E_FUNC:
      if (expr->func.bound.size > 0) {
        BUFFER_ITER(expr->func.bound, sbIrVariable*, var) {
          if ((*var)->is_upvalue) {
            /* BC_LD_UPREF: closed over variables are always on
             * the heap, so all upval refs are rrefs */
            EMIT(BC_LD_UPREF);
          } else {
            /* BC_LD_RREF: everything we are closing over from the
             * current scope needs to move to the heap */
            EMIT(BC_LD_RREF);
          }
          EARG((*var)->slot_id);
        }
      }
      EMIT(BC_LD_BLK);
      EARG(expr->func.chunk->id);
      if (expr->func.bound.size > 0) {
        EMIT(BC_CLOSURE);
        /* record number of variables we are closing over */
        EARG(expr->func.bound.size / sizeof(sbIrVariable*));
      }
      break;
    case IR_E_LIST:
      compile_list(cm, expr);
      EMIT(BC_LIST_GATHER);
      break;
    case IR_E_HASH:
      compile_hash(cm, expr);
      EMIT(BC_HASH_GATHER);
      break;
    case IR_E_VALUE:
      if (expr->value.type == IT_NIL) {
        EMIT(BC_LD_NIL);
      } else if (expr->value.type == IT_BOOLEAN && expr->value.boolean) {
        EMIT(BC_LD_TRUE);
      } else if (expr->value.type == IT_BOOLEAN) {
        EMIT(BC_LD_FALSE);
      } else if (expr->value.type == IT_INTEGER && expr->value.integer < (2 << 8)) {
        EMIT(BC_LD_IMM);
        EARG(expr->value.integer);
      } else {
        /* TODO Deduplicate constants */
        u32 constant_index = sbVmCompiler_add_constant(cm, &expr->value);
        EMIT(BC_LD_CONST);
        EARG(constant_index);
      }
      break;
    default:
      PANIC("haven't implemented this expr type!");
  }
}

/* when we see an expression of the form '&expr', we have to
 * handle this specially depending on what 'expr' is (and sometimes
 * we just aren't allowed to do it). this is also used for compiling
 * certain indexing structures on the left side of assignment operators */
void compile_ref(sbVmCompiler *cm, sbIrExpr *expr, flag is_lref) {
  if (expr->type == IR_E_VAR) {
    if (expr->var->is_upvalue) {
      EMIT(BC_LD_UPREF);
    } else if (is_lref) {
      EMIT(BC_LD_LREF);
    } else {
      EMIT(BC_LD_RREF);
    }
    EARG(expr->var->slot_id);
  } else if (expr->type == IR_E_OP && expr->op.type == AST_OP_INDEX) {
    compile_expr(cm, expr->op.right);         /* index we are using */
    if (is_lref) {
      compile_maybe_ref(cm, expr->op.left, TRUE, BC_OP_INDEXLREF, BC_OP_INDEXLREF_IND);
    } else {
      compile_maybe_ref(cm, expr->op.left, FALSE, BC_OP_INDEXRREF, BC_OP_INDEXRREF_IND);
    }
  } else if (expr->type == IR_E_DOT) {
    compile_expr(cm, expr->dot.param);
    EMIT(BC_LD_IMM);
    EARG(1);
    compile_maybe_ref(cm, expr->dot.target, is_lref, BC_DOT, BC_DOT_IND);
  } else {
    PANIC("This type of reference operation is not supported!");
  }
}

void compile_maybe_ref(sbVmCompiler *cm, sbIrExpr *expr, flag is_lref, sbOpcode op, sbOpcode op_ind) {
  if (is_implicit_ref(expr->dot.target)) {
    compile_ref(cm, expr, is_lref);
    EMIT(op_ind);
  } else if (expr->type == IR_E_OP && expr->op.type == AST_OP_DEREF) {
    /* explicit *<whatever>: do thing indirectly 'through' the reference,
     * instead of dereferencing and making a copy */
    compile_expr(cm, expr->op.left);
    EMIT(op_ind);
  } else {
    compile_expr(cm, expr);
    EMIT(op);
  }
}

void compile_op(sbVmCompiler *cm, sbAstOp op) {
  switch (op) {
    case AST_OP_ADD: EMIT(BC_OP_ADD); break;
    case AST_OP_SUB: EMIT(BC_OP_SUB); break;
    case AST_OP_MUL: EMIT(BC_OP_MUL); break;
    case AST_OP_FLDIV: EMIT(BC_OP_FLDIV); break;
    case AST_OP_MOD: EMIT(BC_OP_MOD); break;
    case AST_OP_EQ: EMIT(BC_OP_EQ); break;
    case AST_OP_NE: EMIT(BC_OP_EQ); EMIT(BC_OP_NOT); break;
    case AST_OP_LT: EMIT(BC_OP_LT); break;
    case AST_OP_GT: EMIT(BC_OP_LE); EMIT(BC_OP_NOT); break;
    case AST_OP_LE: EMIT(BC_OP_LE); break;
    case AST_OP_GE: EMIT(BC_OP_LT); EMIT(BC_OP_NOT); break;
    case AST_OP_NOT: EMIT(BC_OP_NOT); break;
    case AST_OP_AND: EMIT(BC_OP_AND); break;
    case AST_OP_OR: EMIT(BC_OP_OR); break;
    case AST_OP_INDEX: EMIT(BC_OP_INDEXVAL); break;
    case AST_OP_RANGEINDEX: EMIT(BC_OP_RANGEINDEX); break;
    case AST_OP_DEREF: EMIT(BC_OP_DEREF); break;
    /* op range is currently only used in rangeindex; just pass them to it directly */
    case AST_OP_RANGE: break;
    case AST_OP_DIVBY: EMIT(BC_OP_MOD); EMIT(BC_LD_IMM); EARG(0); EMIT(BC_OP_EQ); break;
    default:
      PANIC("unknown operation! (%lld / %c)", (long long)op, op);
  }
}
