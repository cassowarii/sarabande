#include "compile/emit.h"

#define STRING1(...) #__VA_ARGS__
#define STRING2(...) STRING1(__VA_ARGS__)
#define EMIT(...) do { printf(STRING2(__VA_ARGS__) "\n"); sbVmCompiler_write_code(cm, (u8[]) { __VA_ARGS__ }, sizeof((u8[]) { __VA_ARGS__ })); } while (0)
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

void compile_chunk(sbVmCompiler *cm, sbIrChunk *chunk) {
  //sbVmCompiler_add_constant(&cm, &HVINT(5));
  //sbVmCompiler_add_constant(&cm, &HVINT(9));

  printf("\n--block--\n");

  int nstmts = chunk->stmts.size / sizeof(sbIrStmt*);
  for (int i = 0; i < nstmts; i++) {
    sbIrStmt *stmt = ((sbIrStmt**)chunk->stmts.data)[i];

    compile_stmt(cm, stmt);
  }
}

/* when compiling one line at a time, we don't know the position
 * of labels we haven't seen yet. we leave a 4-byte slot for a blank
 * address, and say "please put the address of this label here later!" */
void record_labelpos(sbVmCompiler *cm, sbIrLabel *label, u32 offset) {
  struct labelpos lp = {
  };

  sbBuffer_append(&cm->label_positions, &lp, sizeof(struct labelpos));
}

void emit_arg(sbVmCompiler *cm, usize number) {
  printf("  arg %zu\n", number);
  u8 buf[9];
  if (number < 256) {
    buf[0] = number;
    sbVmCompiler_write_code(cm, buf, 1);
  } else if (number < 65536) {
    buf[0] = BC_LONG_NUM;
    buf[1] = number >> 8;
    buf[2] = number & 0xFF;
    sbVmCompiler_write_code(cm, buf, 3);
  } else if (number < (1LL << 32)) {
    buf[0] = BC_VLONG_NUM;
    buf[1] = (number >> 24) & 0xFF;;
    buf[2] = (number >> 16) & 0xFF;
    buf[3] = (number >> 8)  & 0xFF;
    buf[4] = number & 0xFF;
    sbVmCompiler_write_code(cm, buf, 5);
  } else {
    buf[0] = BC_VVLONG_NUM;
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

void compile_stmt(sbVmCompiler *cm, sbIrStmt *stmt) {
  printf("%zu ", sbVmCompiler_get_position(cm));
  switch (stmt->type) {
    case IR_S_EXPR:
      compile_expr(cm, stmt->expr);
      EMIT(BC_POP);
      break;
    case IR_S_JUMP:
      if (stmt->jump.label->found_yet) {
        if (stmt->jump.condition == NULL) {
          EMIT(BC_JMP);
        } else {
          compile_expr(cm, stmt->jump.condition);
          if (stmt->jump.inverted) {
            EMIT(BC_JF);
          } else {
            EMIT(BC_JT);
          }
        }
        EARG(stmt->jump.label->block_position);
      } else {
        printf("haven't implemented forward jump yet!\n");
      }
      break;
    case IR_S_LABEL:
      stmt->label->found_yet = TRUE;
      stmt->label->block_position = sbVmCompiler_get_position(cm);
      printf("\n");
      break;
    case IR_S_RETURN:
      if (stmt->expr) {
        compile_expr(cm, stmt->expr);
      }
      EMIT(BC_RET);
      break;
    default:
      printf("haven't implemented this yet!\n");
  }
}

void compile_op(sbVmCompiler *cm, sbAstOp op);
void compile_expr(sbVmCompiler *cm, sbIrExpr *expr) {
  switch(expr->type) {
    case IR_E_OP:
      compile_expr(cm, expr->op.left);
      if (expr->op.right) {
        compile_expr(cm, expr->op.right);
      }
      compile_op(cm, expr->op.type);
      break;
    default:
      printf("(compile an expr)\n");
  }
}

void compile_op(sbVmCompiler *cm, sbAstOp op) {
  switch (op) {
    case AST_OP_ADD: EMIT(BC_OP_ADD); break;
    case AST_OP_SUB: EMIT(BC_OP_SUB); break;
    default:
      printf("unknown operation!\n");
  }
}
