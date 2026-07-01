#include "vm/exec.h"

#include "vm/operations.h"

void call_block(hVm vm, usize block_id);
void execute_instruction(hVm vm);

void sbVm_initialize(hVm vm, usize stacksize, usize rstacksize, flag debugmode) {
  *vm = (sbVm) {0};
  vm->stack = malloc(stacksize);
  vm->stacksize = stacksize;
  vm->rstack = malloc(rstacksize);
  vm->rstacksize = rstacksize;

  vm->sp = vm->stack;
  vm->fp = (sbVmStackFrame*)vm->rstack;
  vm->rp = vm->rstack;

  vm->debugmode = debugmode;
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
    if (vm->debugmode) {
      debug("Stack: ");
      for (u8 *p = vm->stack; p < vm->sp; p++) {
        debug("%02X ", *p);
      }
      debug("\n");
    }
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
  for (usize i = 0; i < vm->fp->num_locals; i++) {
    sbV_release(&vm->fp->locals[i]);
  }

  sbVmStackFrame *frame = (sbVmStackFrame*)vm->fp;

  if (frame->return_addr == NULL) {
    /* returning to address 0 means quit */
    vm->running = FALSE;
  } else {
    vm->fp = frame->last_fp;
    vm->rp = frame->last_rp;
    vm->ip = frame->return_addr;
  }
}

void debug_print_hv(const hV *value) {
  debug("%016llx %016llx\n", *(u64*)value, *((u64*)value+1));
}

void store_local(hVm vm, usize local_index, const hV *value) {
  /* release whatever was in the slot before */
  sbV_release(&vm->fp->locals[local_index]);
  sbV_retain(value);
  vm->fp->locals[local_index] = *value;
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

hV *npop_stack(hVm vm, usize count) {
  vm->sp -= count * sizeof(hV);
  for (usize i = 0; i < count; i++) {
    sbV_release(&((hV*)vm->sp)[i]);
  }
  return (hV*)vm->sp;
}

hV *peek_stack(hVm vm, usize offset) {
  return (hV*)(vm->sp - (offset + 1) * sizeof(hV));
}

sbOpcode get_opcode(hVm vm) {
  if (vm->ip >= vm->fp->block->bytecode_end) PANIC("execution outside defined code area");
  return *(vm->ip++);
}

void next_byte(hVm vm, u64 *result) {
  u8 byte = *(vm->ip++);
  if (vm->debugmode) debug("%02X ", byte);
  *result <<= 8;
  *result |= byte;
}

/* small numbers that are parameters to opcodes can
 * just be encoded directly as 1 byte. numbers of 16-bit
 * scale can be encoded as FC + bytebyte, numbers of
 * 32 bit scale can be FD + bytebytebytebyte,
 * 64 bit can be FE + bytebytebytebytebytebytebytebyte,
 * in big-endian format */
i64 get_param(hVm vm) {
  u64 result = *((u8*)vm->ip++);
  if (vm->debugmode) debug("%02llX ", result);
  i64 signed_result = result;

  if (result == BC_LONG_NUM) {
    result = 0;
    next_byte(vm, &result);
    next_byte(vm, &result);
    signed_result = result;
    if (result > (1 << 15)) signed_result = result - (1 << 16);
  } else if (result == BC_VLONG_NUM) {
    result = 0;
    next_byte(vm, &result); // 0
    next_byte(vm, &result); // 1
    next_byte(vm, &result); // 2
    next_byte(vm, &result); // 3
    signed_result = result;
    if (result > (1L << 31)) signed_result = result - (1L << 32);
  } else if (result == BC_VVLONG_NUM) {
    result = 0;
    next_byte(vm, &result); // 0
    next_byte(vm, &result); // 1
    next_byte(vm, &result); // 2
    next_byte(vm, &result); // 3
    next_byte(vm, &result); // 4
    next_byte(vm, &result); // 5
    next_byte(vm, &result); // 6
    next_byte(vm, &result); // 7
    signed_result = (i64)result;
  }

  return signed_result;
}

/* execute one instruction! wow! */
void execute_instruction(hVm vm) {
  sbOpcode op = get_opcode(vm);
  if (vm->debugmode) debug("op %02X ", op);
  u64 param;
  hV *v, *w, res;

  switch (op) {
    case BC_NOP:
      break;
    case BC_HALT:
      vm->running = FALSE;
      break;
    case BC_LD_IMM:
      param = get_param(vm);
      push_stack(vm, &HVINT(param));
      break;
    case BC_LD_CONST:
      param = get_param(vm);
      push_stack(vm, &vm->fp->block->constants[param]);
      break;
    case BC_LD_CTX:
      PANIC("todo");
    case BC_LD_VAR:
      param = get_param(vm);
      push_stack(vm, &vm->fp->locals[param]);
      break;
    case BC_LD_UPVAL:
      /* Hey, was there upval in there? */
      PANIC("todo");
    case BC_LD_BLK:
      param = get_param(vm);
      push_stack(vm, &HVFUNC(param));
      break;
    case BC_LD_NIL:
      push_stack(vm, &HVNIL);
      break;
    case BC_ST_VAR:
      param = get_param(vm);
      v = peek_stack(vm, 0);
      store_local(vm, param, v);
      pop_stack(vm);
      break;
    case BC_ST_UPVAL:
      PANIC("todo");
    case BC_ST_ARG:
      param = get_param(vm);
      v = pop_stack(vm); /* argument count */
      if (v->type != IT_INTEGER) {
        /* internal error! this should be generated correctly */
        PANIC("calling convention violation: number of args should be an integer");
      }
      w = pop_stack(vm); /* argument value */
      store_local(vm, param, w);
      /* we track the number of arguments remaining, for variadic functions later */
      v->integer --;
      if (v->integer > 0) {
        /* if last integer, don't put the 0 count back on the stack */
        push_stack(vm, v);
      }
      break;
    case BC_POP:
      v = pop_stack(vm);
      break;
    case BC_NPOP:
      param = get_param(vm);
      npop_stack(vm, param);
      break;
    case BC_CALL:
      v = pop_stack(vm);
      if (v->type != IT_FUNCTION) {
        /* We need to figure out exception support or some such.
         * User error should not panic. */
        PANIC("attempt to call a non-function value");
      }
      call_block(vm, v->data);
      break;
    case BC_NUMARG:
      param = get_param(vm);
      v = pop_stack(vm);
      if (v->type != IT_INTEGER) {
        /* internal error! this should be generated correctly */
        PANIC("calling convention violation: number of args should be an integer");
      }
      if (v->integer != param) {
        /* This should be an exception. */
        PANIC("wrong number of arguments passed to function.");
      }
      push_stack(vm, v);
      break;
    case BC_JMP:
      param = get_param(vm);
      vm->ip = &vm->fp->block->bytecode[param];
      break;
    case BC_JT:
      param = get_param(vm);
      v = pop_stack(vm);
      if (!sbV_c_falsy(v)) {
        vm->ip = &vm->fp->block->bytecode[param];
      }
      break;
    case BC_JF:
      param = get_param(vm);
      v = pop_stack(vm);
      if (sbV_c_falsy(v)) {
        vm->ip = &vm->fp->block->bytecode[param];
      }
      break;
    case BC_RET:
      return_from_block(vm);
      break;
    case BC_SEND:
      PANIC("todo");
    case BC_OP_EQ:
      v = peek_stack(vm, 1);
      w = peek_stack(vm, 0);
      res = sbV_eq(v, w);
      npop_stack(vm, 2);
      push_stack(vm, &res);
      break;
    case BC_OP_NOT:
      v = pop_stack(vm);
      if (sbV_c_falsy(v)) {
        push_stack(vm, &HVBOOL(TRUE));
      } else {
        push_stack(vm, &HVBOOL(FALSE));
      }
      break;
    case BC_OP_LT:
      v = peek_stack(vm, 1);
      w = peek_stack(vm, 0);
      res = sbV_lt(v, w);
      npop_stack(vm, 2);
      push_stack(vm, &res);
      break;
    case BC_OP_LE:
      v = peek_stack(vm, 1);
      w = peek_stack(vm, 0);
      res = sbV_le(v, w);
      npop_stack(vm, 2);
      push_stack(vm, &res);
      break;
    case BC_OP_ADD:
      v = peek_stack(vm, 1);
      w = peek_stack(vm, 0);
      res = sbV_add(v, w);
      npop_stack(vm, 2);
      push_stack(vm, &res);
      break;
    case BC_OP_SUB:
      v = peek_stack(vm, 1);
      w = peek_stack(vm, 0);
      res = sbV_sub(v, w);
      npop_stack(vm, 2);
      push_stack(vm, &res);
      break;
    case BC_OP_MUL:
      v = peek_stack(vm, 1);
      w = peek_stack(vm, 0);
      res = sbV_mul(v, w);
      npop_stack(vm, 2);
      push_stack(vm, &res);
      break;
    case BC_OP_DIV:
      PANIC("todo");
    case BC_OP_FLDIV:
      v = peek_stack(vm, 1);
      w = peek_stack(vm, 0);
      res = sbV_floordiv(v, w);
      npop_stack(vm, 2);
      push_stack(vm, &res);
      break;
    case BC_OP_NEG:
    case BC_OP_MOD:
    case BC_OP_POW:
      PANIC("todo");
    case BC_OP_INCR:
      v = peek_stack(vm, 0);
      res = sbV_incr(v);
      pop_stack(vm);
      push_stack(vm, &res);
      break;
    case BC_OP_DECR:
      v = peek_stack(vm, 0);
      res = sbV_decr(v);
      pop_stack(vm);
      push_stack(vm, &res);
      break;
    case BC_OP_INDEX:
      v = peek_stack(vm, 1);
      w = peek_stack(vm, 0);
      res = sbV_index(v, w);
      npop_stack(vm, 2);
      push_stack(vm, &res);
      break;
    case BC_OP_DEREF:
      PANIC("todo");
    case BC_ALLOC_VARS:
      /* TODO check for overflow */
      param = get_param(vm);
      /* have to set new rstack space to 0 so we don't accidentally decrement
       * the ref count of variables from a previous stack frame (or of just garbage)  */
      memset(vm->rp, 0, param * sizeof(hV));
      /* we allocate one more variable at 0 for internal use */
      vm->rp += (param + 1) * sizeof(hV);
      vm->fp->num_locals += param;
      break;
    case BC_LIST_GATHER:
      v = pop_stack(vm);
      if (v->type != IT_INTEGER) {
        /* internal error! this should be generated correctly */
        PANIC("internal violation: LIST_GATHER should receive an integer on top of stack");
      }
      param = v->integer;
      res = sbV_empty_list(param);
      while (param > 0) {
        w = pop_stack(vm);
        sbV_append(&res, w);
        param --;
      }
      push_stack(vm, &res);
      break;
    case BC_HASH_GATHER:
      PANIC("todo");
    case BC_LONG_NUM:
    case BC_VLONG_NUM:
      PANIC("illegal opcode $%02X at position $%016zX", op, (usize)vm->ip);
    default:
      PANIC("unrecognized opcode $%02X at position $%016zX", op, (usize)vm->ip);
  }
  if (vm->debugmode) debug("\n");
}
