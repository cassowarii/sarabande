#include "vm/exec.h"

#include "data/integer.h"

void call_block(hVm vm, usize block_id);
void execute_instruction(hVm vm);

void sbVm_initialize(hVm vm, usize stacksize, usize rstacksize) {
  *vm = (sbVm) {0};
  vm->stack = malloc(stacksize);
  vm->stacksize = stacksize;
  vm->rstack = malloc(rstacksize);
  vm->rstacksize = rstacksize;

  vm->sp = vm->stack;
  vm->fp = (sbVmStackFrame*)vm->rstack;
  vm->rp = vm->rstack;
}

void sbVm_deinitialize(hVm vm) {
  free(vm->stack);
  free(vm->rstack);
  *vm = (sbVm) {0};
}

sbVmStatus sbVm_execute(hVm vm, sbVmProgram *pm) {
  vm->program = pm;
  vm->running = TRUE;

  /* Let's start at the very beginning.
   * A very good place to start. */
  call_block(vm, 0);

  while (vm->running) {
    execute_instruction(vm);
  }

  return VM_STAT_SUCCESS;
}

/* --- */

void call_block(hVm vm, usize block_id) {
  sbVmBlock *blk = &vm->program->blocks[block_id];

  sbVmStackFrame frame = {
    .return_addr = vm->ip,
    .last_fp = vm->fp,
    .last_rp = vm->rp,
    .block = blk,
  };
  *(sbVmStackFrame*)vm->rp = frame;
  vm->fp = (sbVmStackFrame*)vm->rp;
  vm->rp += sizeof(sbVmStackFrame);
  vm->ip = &blk->bytecode[0];
}

void return_from_block(hVm vm) {
  sbVmStackFrame *frame = (sbVmStackFrame*)vm->fp;

  vm->fp = frame->last_fp;
  vm->rp = frame->last_rp;
  vm->ip = frame->return_addr;
}

void push_stack(hVm vm, const hV *value) {
  sbV_retain(value);
  *(hV*)vm->sp = *value;
  vm->sp += sizeof(hV);
}

hV *pop_stack(hVm vm) {
  vm->sp -= sizeof(hV);
  sbV_release((hV*)vm->sp);
  return (hV*)vm->sp;
}

sbOpcode get_opcode(hVm vm) {
  return *(vm->ip++);
}

/* small numbers that are parameters to opcodes can
 * just be encoded directly as 1 byte. numbers of 16-bit
 * scale can be encoded as FE + byte + byte, numbers of
 * 32 bit scale can be FF + byte + byte + byte + byte,
 * in big-endian format */
u32 get_param(hVm vm) {
  u32 result = *((u8*)vm->ip++);

  if (result == BC_LONG_NUM) {
    result = *(vm->ip++);
    result <<= 8;
    result |= *(vm->ip++);
  } else if (result == BC_VLONG_NUM) {
    result = *(vm->ip++);
    result <<= 8;
    result |= *(vm->ip++);
    result <<= 8;
    result |= *(vm->ip++);
    result <<= 8;
    result |= *(vm->ip++);
  }

  return result;
}

/* execute one instruction! wow! */
void execute_instruction(hVm vm) {
  sbOpcode op = get_opcode(vm);
  u32 param;
  hV *v, a, b, c;

  switch (op) {
    case BC_NOP:
      return;
    case BC_HALT:
      vm->running = FALSE;
      return;
    case BC_LD_IMM:
      param = get_param(vm);
      push_stack(vm, &HVINT(param));
      return;
    case BC_LD_CONST:
      param = get_param(vm);
      push_stack(vm, &vm->fp->block->constants[param]);
      return;
    case BC_LD_CTX:
      PANIC("todo");
    case BC_LD_VAR:
      param = get_param(vm);
      push_stack(vm, &vm->fp->locals[param]);
      return;
    case BC_LD_UPVAL:
      /* Hey, was there upval in there? */
      PANIC("todo");
    case BC_ST_VAR:
      param = get_param(vm);
      vm->fp->locals[param] = *pop_stack(vm);
      return;
    case BC_ST_UPVAL:
      PANIC("todo");
    case BC_POP:
      v = pop_stack(vm);
      sbV_release(v);
      return;
    case BC_CALL:
      param = get_param(vm);
      call_block(vm, param);
      return;
    case BC_JMP:
      param = get_param(vm);
      vm->ip = &vm->fp->block->bytecode[param];
      return;
    case BC_RET:
      return_from_block(vm);
      return;
    case BC_SEND:
      PANIC("todo");
    case BC_OP_ADD:
      b = *pop_stack(vm);
      a = *pop_stack(vm);
      if (a.type == IT_INTEGER && b.type == IT_INTEGER) {
        c = sbV_int(sbInteger_sum(a.integer, b.integer));
        push_stack(vm, &c);
      } else {
        PANIC("todo");
      }
      return;
    case BC_OP_SUB:
      b = *pop_stack(vm);
      a = *pop_stack(vm);
      if (a.type == IT_INTEGER && b.type == IT_INTEGER) {
        c = sbV_int(sbInteger_diff(a.integer, b.integer));
        push_stack(vm, &c);
      } else {
        PANIC("todo");
      }
      return;
    case BC_OP_MUL:
      b = *pop_stack(vm);
      a = *pop_stack(vm);
      if (a.type == IT_INTEGER && b.type == IT_INTEGER) {
        c = sbV_int(sbInteger_mul(a.integer, b.integer));
        push_stack(vm, &c);
      } else {
        PANIC("todo");
      }
      return;
    case BC_OP_DIV:
      PANIC("todo");
    case BC_OP_FLDIV:
      b = *pop_stack(vm);
      a = *pop_stack(vm);
      if (a.type == IT_INTEGER && b.type == IT_INTEGER) {
        c = sbV_int(sbInteger_floordiv(a.integer, b.integer));
        push_stack(vm, &c);
      } else {
        PANIC("todo");
      }
      return;
    case BC_OP_NEG:
    case BC_OP_MOD:
    case BC_OP_POW:
    case BC_OP_DEREF:
      PANIC("todo");
    case BC_ALLOC_VARS:
      param = get_param(vm);
      vm->rp += param * sizeof(hV);
      return;
    case BC_LIST_NEW:
    case BC_HASH_NEW:
    case BC_LIST_ADD:
    case BC_HASH_ADD:
      PANIC("todo");
    case BC_LONG_NUM:
    case BC_VLONG_NUM:
      PANIC("illegal opcode $%02X at position $%08zX\n", op, (usize)vm->ip);
    default:
      PANIC("unrecognized opcode $%02X at position $%08zX\n", op, (usize)vm->ip);
  }
}
