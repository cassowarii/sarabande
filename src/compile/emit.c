#include "compile/emit.h"

#define STRING1(...) #__VA_ARGS__
#define STRING2(...) STRING1(__VA_ARGS__)
#define EMIT(...) do { debug(STRING2(__VA_ARGS__) "\n"); sbVmCompiler_write_code(cm, (u8[]) { __VA_ARGS__ }, sizeof((u8[]) { __VA_ARGS__ })); } while (0)
#define EARG(x) (emit_arg(cm, x))

struct labelpos {
  sbIrLabel *label;
  u32 offset;
};

void compile_chunk(sbVmCompiler *cm, sbIrChunk *chunk);
void compile_stmt(sbVmCompiler *cm, sbIrStmt *stmt);
void compile_expr(sbVmCompiler *cm, sbIrExpr *expr);

void sbEmit_compile_program(sbVmProgram *vp, sbIrProgram *ir) {
  sbVmCompiler cm = sbVmCompiler_create(4096, 4096);

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

void emit_arg(sbVmCompiler *cm, i64 actual_number) {
  debug("    arg %lld\n", actual_number);
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
    buf[1] = number >> 8;
    buf[2] = number & 0xFF;
    sbVmCompiler_write_code(cm, buf, 3);
  } else if (-(1L << 31) < actual_number && actual_number < (1L << 31)) {
    buf[0] = BC_VLONG_NUM;
    if (actual_number < 0) {
      number = actual_number + (1LL << 32);
    }
    buf[1] = (number >> 24) & 0xFF;;
    buf[2] = (number >> 16) & 0xFF;
    buf[3] = (number >> 8)  & 0xFF;
    buf[4] = number & 0xFF;
    sbVmCompiler_write_code(cm, buf, 5);
  } else {
    buf[0] = BC_VVLONG_NUM;
    number = (u64)actual_number;
    buf[1] = (number >> 56) & 0xFF;;
    buf[2] = (number >> 48) & 0xFF;;
    buf[3] = (number >> 40) & 0xFF;;
    buf[4] = (number >> 32) & 0xFF;;
    buf[5] = (number >> 24) & 0xFF;;
    buf[6] = (number >> 16) & 0xFF;
    buf[7] = (number >> 8)  & 0xFF;
    buf[8] = number & 0xFF;
    sbVmCompiler_write_code(cm, buf, 9);
  }
}


void compile_chunk(sbVmCompiler *cm, sbIrChunk *chunk) {
  debug("\n--block %d--\n", chunk->id);

  int nstmts = chunk->stmts.size / sizeof(sbIrStmt*);
  if (chunk->id > 0) {
    /* don't check args for initial block
     * (note: when we have modules or whatever, this might
     * be an incorrect way to do this) */
    EMIT(BC_NUMARG);
    EARG(chunk->num_args);
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
    debug("now we know that the jump at %d should go to %d\n", lp.offset - 2, position);
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

void compile_stmt(sbVmCompiler *cm, sbIrStmt *stmt) {
  debug("%3zu ", sbVmCompiler_get_position(cm));
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
        debug("%3zu ", sbVmCompiler_get_position(cm));
        if (stmt->jump.inverted) {
          EMIT(BC_JF);
        } else {
          EMIT(BC_JT);
        }
      }
      if (stmt->jump.label->found_yet) {
        EARG(stmt->jump.label->block_position);
      } else {
        debug("<forward jump>\n");
        EMIT(BC_VLONG_NUM);
        /* we have to remember to come back and fill in the address later when we
         * know where this label is! */
        record_labelpos(cm, stmt->jump.label, sbVmCompiler_get_position(cm));
        /* leave behind four zeroes at this offset that we can later put the
         * address into */
        EMIT(0, 0, 0, 0);
      }
      break;
    case IR_S_LABEL:
      stmt->label->found_yet = TRUE;
      stmt->label->block_position = sbVmCompiler_get_position(cm);
      debug("\n");
      break;
    case IR_S_ASSIGN:
      compile_expr(cm, stmt->assign.expr);
      EMIT(BC_ST_VAR);
      EARG(stmt->assign.var->slot_id);
      break;
    case IR_S_ARG:
      EMIT(BC_ST_ARG);
      EARG(stmt->arg.var->slot_id);
      break;
    case IR_S_RETURN:
      if (stmt->expr) {
        compile_expr(cm, stmt->expr);
      }
      EMIT(BC_RET);
      break;
    default:
      debug("haven't implemented this yet!\n");
  }
}

void compile_op(sbVmCompiler *cm, sbAstOp op);
void compile_expr(sbVmCompiler *cm, sbIrExpr *expr) {
  sbIrExpr *E1;
  int count = 0;
  switch(expr->type) {
    case IR_E_OP:
      compile_expr(cm, expr->op.left);
      if (expr->op.right) {
        compile_expr(cm, expr->op.right);
      }
      compile_op(cm, expr->op.type);
      break;
    case IR_E_CALL:
      E1 = expr->call.param;
      while (E1) {
        count ++;
        compile_expr(cm, E1->param.this);
        E1 = E1->param.next;
      }
      EMIT(BC_LD_IMM);
      EARG(count); /* calling convention: store argument count on stack */
      compile_expr(cm, expr->call.func);
      EMIT(BC_CALL);
      break;
    case IR_E_VAR:
      EMIT(BC_LD_VAR);
      EARG(expr->var->slot_id);
      break;
    case IR_E_FUNC:
      EMIT(BC_LD_BLK);
      EARG(expr->func->id);
      break;
    case IR_E_VALUE:
      if (expr->value.type == IT_NIL) {
        EMIT(BC_LD_NIL);
      } else if (expr->value.type == IT_BOOLEAN && expr->value.boolean) {
        EMIT(BC_LD_TRUE);
      } else if (expr->value.type == IT_BOOLEAN) {
        EMIT(BC_LD_FALSE);
      } else if (expr->value.type == IT_INTEGER && expr->value.integer < (2 << 16)) {
        EMIT(BC_LD_IMM);
        EARG(expr->value.integer);
      } else {
        u32 constant_index = sbVmCompiler_add_constant(cm, &expr->value);
        EMIT(BC_LD_CONST);
        EARG(constant_index);
      }
      break;
    default:
      debug("(compile an expr)\n");
  }
}

void compile_op(sbVmCompiler *cm, sbAstOp op) {
  switch (op) {
    case AST_OP_ADD: EMIT(BC_OP_ADD); break;
    case AST_OP_SUB: EMIT(BC_OP_SUB); break;
    case AST_OP_MUL: EMIT(BC_OP_MUL); break;
    case AST_OP_FLDIV: EMIT(BC_OP_FLDIV); break;
    case AST_OP_EQ: EMIT(BC_OP_EQ); break;
    case AST_OP_NE: EMIT(BC_OP_EQ, BC_OP_NOT); break;
    case AST_OP_LT: EMIT(BC_OP_LT); break;
    case AST_OP_GT: EMIT(BC_OP_LE, BC_OP_NOT); break;
    case AST_OP_LE: EMIT(BC_OP_LE); break;
    case AST_OP_GE: EMIT(BC_OP_LT, BC_OP_NOT); break;
    default:
      debug("unknown operation!\n");
  }
}
