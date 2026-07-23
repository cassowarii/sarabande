#include "vm/exec.h"

#include "vm/operations.h"
#include "data/list.h"
#include "data/closure.h"
#include "data/symbol.h"
#include "lib/lib.h"
#include "lib/sentinel.h"

void call_block(hVm vm, usize block_id, hClosure closure);
void call_builtin(hVm vm, hVal *to_call);
void call_bound_method(hVm vm, hVal *to_call);
void return_from_block(hVm vm);
void execute_instruction(hVm vm);
void push_stack(hVm vm, hVal *value);
void push_stack_immediate(hVm vm, const hVal *value);
hVal *pop_stack(hVm vm);
hVal *npop_stack(hVm vm, usize count);
hVal *peek_stack(hVm vm, isize offset);
void swap_stack_top(hVm vm);
void print_stack(hVm vm);

void sbVm_initialize(hVm vm, usize stacksize, usize rstacksize, flag debugmode) {
  *vm = (sbVm) {
    .stacksize = stacksize,
    .rstacksize = rstacksize,
    .debugmode = debugmode,
  };

  vm->vstack = malloc(stacksize);
  vm->vsp = vm->vstack;
  vm->rstack = malloc(rstacksize);
  vm->rsp = vm->rstack;
  vm->fp = (sbVmStackFrame*)vm->rstack;

  vm->debugmode = debugmode;
}

void sbVm_deinitialize(hVm vm) {
  free(vm->vstack);
  free(vm->rstack);
  *vm = (sbVm) {0};
}

sbVmStatus sbVm_execute(hVm vm, sbVmProgram *pm) {
  vm->program = pm;
  vm->running = TRUE;

  /* Let's start at the very beginning.
   * A very good place to start. */
  call_block(vm, 0, 0);

  while (vm->running) {
    execute_instruction(vm);
    if (vm->debugmode) {
      print_stack(vm);
    }
  }

  return VM_STAT_SUCCESS;
}

void sbVm_push(hVm vm, hVal *value) {
  push_stack(vm, value);
}

void sbVm_push_immediate(hVm vm, hVal *value) {
  push_stack_immediate(vm, value);
}

hVal *sbVm_pop(hVm vm) {
  return pop_stack(vm);
}

hVal *sbVm_npop(hVm vm, usize how_many) {
  return npop_stack(vm, how_many);
}

hVal *sbVm_peek(hVm vm, usize where) {
  return peek_stack(vm, where);
}

void sbVm_swap(hVm vm) {
  swap_stack_top(vm);
}

void sbVm_call_func(hVm vm, hVal *func) {
  if (func->type == IT_BUILTIN) {
    call_builtin(vm, func);
  } else if (func->type <= 0) {
    /* intrinsic type: resolve a property on it */
    push_stack(vm, func);
    sbLib_resolve_property(vm);
  } else if (func->type & IT_FLAG_BOUND_METHOD) {
    call_bound_method(vm, func);
  } else {
    call_block(vm, func->type, func->closure);
  }
}

void sbVm_transfer_to_func(hVm vm, hVal *func) {
  /* tail call */
  return_from_block(vm);
  sbVm_call_func(vm, func);
}

void sbVm_call_c_func(hVm vm, sbRuntimeCFunc func) {
  sbVmStackFrame frame = {
    .return_addr = vm->ip,
    .last_fp = vm->fp,
    .last_rp = vm->rsp,
    .is_c_func = TRUE,
    .c_func = func,
  };
  *(sbVmStackFrame*)vm->rsp = frame;
  vm->fp = (sbVmStackFrame*)vm->rsp;
  vm->rsp += sizeof(sbVmStackFrame);

  sbCFuncStatus result = func(vm, TRUE);

  if (result == CFUNC_END) {
    /* if c_func didn't call another callback, return from it */
    return_from_block(vm);
  }
}

void sbVm_request_var_space(hVm vm, usize amount) {
  /* have to set new rstack space to 0 so we don't accidentally decrement
   * the ref count of variables from a previous stack frame (or of just garbage)  */
  memset(vm->rsp, 0, amount * sizeof(hVal));
  vm->rsp += amount * sizeof(hVal);
  vm->fp->num_locals += amount;
}

void sbVm_print_stack(hVm vm) {
  print_stack(vm);
}

/* --- */

void print_val(hVal *p) {
  if (p->type == IT_NIL) {
    debug("nil");
  } else if (p->type == IT_BOOLEAN && p->boolean) {
    debug("true");
  } else if (p->type == IT_BOOLEAN) {
    debug("false");
  } else if (p->type == IT_REF) {
    debug("&(");
    print_val(sbVar_get_value_ptr(sbVar_deref(p)));
    debug(")");
  } else if (p->type == IT_SYMBOL) {
    debug(":%s", sbSymbol_name(p->symbol));
  } else if (p->type == IT_INTEGER) {
    debug("%lld", (long long)p->integer);
  } else if (p->type == IT_LIST) {
    debug("[...]");
  } else if (p->type == IT_HASH) {
    debug("{...}");
  } else if (p->type > 0 && (p->type & IT_FLAG_BOUND_METHOD)) {
    debug("(bound method #%lld:%lld)", (long long)p->type, (long long)(p->closure & ~0x80000000000000));
  } else if (p->type > 0 && p->closure > 0) {
    debug("(closure #%lld:%lld)", (long long)p->type, (long long)(p->closure & ~0x8000000000000000));
  } else if (p->type > 0) {
    debug("(block #%lld)", (long long)p->type);
  } else {
    debug("%16llx %16llx", (long long)p->type, (long long)p->data);
  }
}

void print_stack(hVm vm) {
  debug("Stack: ");
  for (hVal *p = (hVal*)vm->vstack; p < (hVal*)vm->vsp; p++) {
    print_val(p);
    debug(" ");
  }
  debug("\n");
}

void call_builtin(hVm vm, hVal *to_call) {
  if (to_call->type != IT_BUILTIN) {
    CHECK("call_builtin can only call builtins!");
  }

  hVal *argc = sbVm_pop(vm);
  if (argc->type != IT_INTEGER) {
    CHECK("argc to builtin needs to be integer!");
  }

  to_call->builtin(vm, argc->integer);
}

void call_block(hVm vm, usize block_id, hClosure closure) {
  sbVmBlock *blk = &vm->program->blocks[block_id];

  sbVmStackFrame frame = {
    .return_addr = vm->ip,
    .last_fp = vm->fp,
    .last_rp = vm->rsp,
    .block_func.block = blk,
    .block_func.closure = closure,
  };
  *(sbVmStackFrame*)vm->rsp = frame;
  vm->fp = (sbVmStackFrame*)vm->rsp;
  vm->rsp += sizeof(sbVmStackFrame);
  vm->ip = &blk->bytecode[0];
}

void call_bound_method(hVm vm, hVal *to_call) {
  if (!(to_call->type & IT_FLAG_BOUND_METHOD)) {
    CHECK("call_bound_method can only call bound methods!");
  }

  /* bound method is just a symbol + a closure containing one variable */
  hSymbol sym = (hSymbol)(to_call->type & ~IT_FLAG_BOUND_METHOD);
  hVal *ref = sbVar_get_attached_ref(to_call);

  /* we should already have parameters, so we just need to line it back up
   * on the stack and call the method again */
  push_stack_immediate(vm, &HVSYM(sym));
  swap_stack_top(vm);
  if (peek_stack(vm, 0)->type != IT_INTEGER) {
    CHECK("bound method call should receive an integer arg count!");
  }
  peek_stack(vm, 0)->integer ++;
  push_stack(vm, ref);

  /* resolve method normally */
  sbLib_resolve_method(vm);
}

void return_from_block(hVm vm) {
  for (usize i = 0; i < vm->fp->num_locals; i++) {
    sbV_release(&vm->fp->locals[i].value);
  }

  sbVmStackFrame *frame = (sbVmStackFrame*)vm->fp;

  if (frame->return_addr == NULL) {
    /* returning to address 0 means quit */
    vm->running = FALSE;
  } else {
    vm->fp = frame->last_fp;
    vm->rsp = frame->last_rp;
    vm->ip = frame->return_addr;
  }
}

void debug_print_hv(const hVal *value) {
  debug("%016llx %016llx\n", *(long long*)value, *((long long*)value+1));
}

void store_local(hVm vm, usize local_index, const hVal *value) {
  sbVar_set_value(&vm->fp->locals[local_index], value);
}

void push_stack(hVm vm, hVal *value) {
  *(hVal*)vm->vsp = *value;
  vm->vsp += sizeof(hVal);
}

void push_stack_immediate(hVm vm, const hVal *value) {
  *(hVal*)vm->vsp = *value;
  vm->vsp += sizeof(hVal);
}

hVal *pop_stack(hVm vm) {
  vm->vsp -= sizeof(hVal);
  return (hVal*)vm->vsp;
}

hVal *npop_stack(hVm vm, usize count) {
  vm->vsp -= count * sizeof(hVal);
  return (hVal*)vm->vsp;
}

void swap_stack_top(hVm vm) {
  hVal *first_v = &((hVal*)vm->vsp)[-1];
  hVal *second_v = &((hVal*)vm->vsp)[-2];

  hVal tmp = *first_v;   /* x2  x1    &v2  &v1  */
  *first_v = *second_v;  /* x2  x1    &v2  &v2  */
  *second_v = tmp;       /* x2  x1    &v1  &v2  */
}

hVal *peek_stack(hVm vm, isize offset) {
  return &((hVal*)(vm->vsp))[-offset - 1];
}

sbOpcode get_opcode(hVm vm) {
  if (vm->ip >= vm->fp->block_func.block->bytecode_end) PANIC("execution outside defined code area");
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
  if (vm->debugmode) debug("%02llX ", (long long)result);
  i64 signed_result;

  if (result == BC_LONG_NUM) {
    result = 0;
    next_byte(vm, &result);
    next_byte(vm, &result);
    signed_result = result;
    if (result & (1 << 15)) signed_result = result - (1 << 16);
  } else if (result == BC_VLONG_NUM) {
    result = 0;
    next_byte(vm, &result); // 0
    next_byte(vm, &result); // 1
    next_byte(vm, &result); // 2
    next_byte(vm, &result); // 3
    signed_result = result;
    if (result & (1L << 31)) signed_result = result - (1L << 32);
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
  } else {
    signed_result = (i64)result;
  }

  return signed_result;
}

/* execute one instruction! wow! */
void execute_instruction(hVm vm) {
  if (vm->fp->is_c_func) {
    /* if still in c func, do that instead of instruction */
    sbCFuncStatus result = vm->fp->c_func(vm, FALSE);

    if (result == CFUNC_END) {
      /* c-function is done */
      return_from_block(vm);
    }

    return;
  }

  sbOpcode op = get_opcode(vm);
#ifdef DEBUG
  if (vm->debugmode) debug("op %s ", g_opcode_names[op]);
#endif
  u64 param;
  usize count;
  hVal *v, *w, *x, res;
  sbVar *vars;

  switch (op) {
    case BC_NOP:
      break;
    case BC_HALT:
      vm->running = FALSE;
      break;
    case BC_LD_IMM:
      param = get_param(vm);
      push_stack_immediate(vm, &HVINT(param));
      break;
    case BC_LD_CONST:
      param = get_param(vm);
      push_stack_immediate(vm, &vm->fp->block_func.block->constants[param]);
      break;
    case BC_LD_CTX:
      param = get_param(vm);
      push_stack_immediate(vm, &HVSYM((hSymbol)param));
      sbLib_resolve_global(vm);
      break;
    case BC_LD_VAR:
      param = get_param(vm);
      res = sbVar_get_value(&vm->fp->locals[param]);
      push_stack(vm, &res);
      break;
    case BC_LD_LREF:
      param = get_param(vm);
      res = sbVar_get_lvalue_ref(&vm->fp->locals[param]);
      push_stack(vm, &res);
      break;
    case BC_LD_RREF:
      param = get_param(vm);
      res = sbVar_get_rvalue_ref(&vm->fp->locals[param]);
      push_stack(vm, &res);
      break;
    case BC_LD_UPVAL:
      /* Hey, was there upval in there? */
      param = get_param(vm);
      v = sbClosure_get_value(vm->fp->block_func.closure, param);
      push_stack(vm, v);
      break;
    case BC_LD_UPREF:
      param = get_param(vm);
      push_stack(vm, &HVREF(sbClosure_get_var(vm->fp->block_func.closure, param)));
      break;
    case BC_LD_BLK:
      param = get_param(vm);
      push_stack_immediate(vm, &HVFUNC(param, 0));
      break;
    case BC_LD_TRUE:
      push_stack_immediate(vm, &HVBOOL(TRUE));
      break;
    case BC_LD_FALSE:
      push_stack_immediate(vm, &HVBOOL(FALSE));
      break;
    case BC_LD_NIL:
      push_stack_immediate(vm, &HVNIL);
      break;
    case BC_ST_VAR:
      param = get_param(vm);
      v = peek_stack(vm, 0);
      store_local(vm, param, v);
      pop_stack(vm);
      break;
    case BC_ST_UPVAL:
      param = get_param(vm);
      v = pop_stack(vm);
      sbClosure_set_value(vm->fp->block_func.closure, param, v);
      break;
    case BC_ST_ARG:
      param = get_param(vm);
      v = pop_stack(vm); /* argument count */
      if (v->type != IT_INTEGER) {
        /* internal error! this should be generated correctly */
        CHECK("calling convention violation: number of args should be an integer");
      }
      w = pop_stack(vm); /* argument value */
      store_local(vm, param, w);
      /* we track the number of arguments remaining, for variadic functions later */
      v->integer --;
      if (v->integer > 0) {
        /* if last integer, don't put the 0 count back on the stack */
        push_stack_immediate(vm, v);
      }
      break;
    case BC_POP:
      pop_stack(vm);
      break;
    case BC_NPOP:
      param = get_param(vm);
      npop_stack(vm, param);
      break;
    case BC_SWAP:
      swap_stack_top(vm);
      break;
    case BC_CALL:
    case BC_DOT:
      v = pop_stack(vm);
      sbVm_call_func(vm, v);
      break;
    case BC_DOT_IND:
    case BC_CALL_IND:
      v = pop_stack(vm);
      if (v->type == IT_REF) {
        vars = sbVar_deref(v);
        w = sbVar_get_value_ptr(vars);
        sbVm_call_func(vm, w);
      } else if (v->type & IT_FLAG_BOUND_METHOD) {
        sbVm_call_func(vm, v);
      } else {
        PANIC("object illegally returned non-reference value");
      }
      break;
    case BC_SEND:
      sbLib_resolve_method(vm);
      break;
    case BC_REF_PUT:
      sbV_refput(vm);
      break;
    case BC_NUMARG:
      param = get_param(vm);
      v = pop_stack(vm);
      if (v->type != IT_INTEGER) {
        /* internal error! this should be generated correctly */
        CHECK("calling convention violation: number of args should be an integer");
      }
      if (v->integer != param) {
        /* This should be an exception. */
        PANIC("wrong number of arguments passed to function.");
      }
      if (v->integer > 0) {
        /* when 0 arguments, leave this off */
        push_stack_immediate(vm, v);
      }
      break;
    case BC_JMP:
      param = get_param(vm);
      vm->ip = &vm->fp->block_func.block->bytecode[param];
      break;
    case BC_JT:
      param = get_param(vm);
      v = pop_stack(vm);
      if (!sbV_c_falsy(v)) {
        vm->ip = &vm->fp->block_func.block->bytecode[param];
      }
      break;
    case BC_JF:
      param = get_param(vm);
      v = pop_stack(vm);
      if (sbV_c_falsy(v)) {
        vm->ip = &vm->fp->block_func.block->bytecode[param];
      }
      break;
    case BC_RET:
      return_from_block(vm);
      break;
    case BC_OP_EQ:
      v = peek_stack(vm, 1);
      w = peek_stack(vm, 0);
      res = sbV_eq(v, w);
      npop_stack(vm, 2);
      push_stack_immediate(vm, &res);
      break;
    case BC_OP_NOT:
      v = pop_stack(vm);
      if (sbV_c_falsy(v)) {
        push_stack_immediate(vm, &HVBOOL(TRUE));
      } else {
        push_stack_immediate(vm, &HVBOOL(FALSE));
      }
      break;
    case BC_OP_LT:
      v = peek_stack(vm, 1);
      w = peek_stack(vm, 0);
      res = sbV_lt(v, w);
      npop_stack(vm, 2);
      push_stack_immediate(vm, &res);
      break;
    case BC_OP_LE:
      v = peek_stack(vm, 1);
      w = peek_stack(vm, 0);
      res = sbV_le(v, w);
      npop_stack(vm, 2);
      push_stack_immediate(vm, &res);
      break;
    case BC_OP_AND:
      v = peek_stack(vm, 1);
      w = peek_stack(vm, 0);
      res = HVBOOL(!(sbV_c_falsy(v) || sbV_c_falsy(w)));
      npop_stack(vm, 2);
      push_stack_immediate(vm, &res);
      break;
    case BC_OP_OR:
      v = peek_stack(vm, 1);
      w = peek_stack(vm, 0);
      res = HVBOOL(!(sbV_c_falsy(v) && sbV_c_falsy(w)));
      npop_stack(vm, 2);
      push_stack_immediate(vm, &res);
      break;
    case BC_OP_ADD:
      v = peek_stack(vm, 1);
      w = peek_stack(vm, 0);
      res = sbV_add(v, w);
      npop_stack(vm, 2);
      push_stack_immediate(vm, &res);
      break;
    case BC_OP_SUB:
      v = peek_stack(vm, 1);
      w = peek_stack(vm, 0);
      res = sbV_sub(v, w);
      npop_stack(vm, 2);
      push_stack_immediate(vm, &res);
      break;
    case BC_OP_MUL:
      v = peek_stack(vm, 1);
      w = peek_stack(vm, 0);
      res = sbV_mul(v, w);
      npop_stack(vm, 2);
      push_stack_immediate(vm, &res);
      break;
    case BC_OP_DIV:
      PANIC("todo");
    case BC_OP_FLDIV:
      v = peek_stack(vm, 1);
      w = peek_stack(vm, 0);
      res = sbV_floordiv(v, w);
      npop_stack(vm, 2);
      push_stack_immediate(vm, &res);
      break;
    case BC_OP_MOD:
      v = peek_stack(vm, 1);
      w = peek_stack(vm, 0);
      res = sbV_mod(v, w);
      npop_stack(vm, 2);
      push_stack_immediate(vm, &res);
      break;
    case BC_OP_NEG:
    case BC_OP_POW:
      PANIC("todo");
    case BC_OP_INCR:
      v = peek_stack(vm, 0);
      sbV_incr(v);
      break;
    case BC_OP_DECR:
      v = peek_stack(vm, 0);
      sbV_decr(v);
      break;
    case BC_OP_INDEXVAL:
      sbV_index_value(vm);
      break;
    case BC_OP_INDEXLREF:
      sbV_index_ref(vm, TRUE, FALSE);
      break;
    case BC_OP_INDEXLREF_IND:
      sbV_index_ref(vm, TRUE, TRUE);
      break;
    case BC_OP_INDEXRREF:
      sbV_index_ref(vm, FALSE, FALSE);
      break;
    case BC_OP_INDEXRREF_IND:
      sbV_index_ref(vm, FALSE, TRUE);
      break;
    case BC_OP_RANGEINDEX:
      v = peek_stack(vm, 2);
      w = peek_stack(vm, 1);
      x = peek_stack(vm, 0);
      res = sbV_rangeindex(v, w, x);
      npop_stack(vm, 3);
      push_stack_immediate(vm, &res);
      break;
    case BC_OP_DEREF:
      sbV_deref(vm);
      break;
    case BC_ALLOC_VARS:
      /* TODO check for overflow */
      param = get_param(vm);
      sbVm_request_var_space(vm, param);
      break;
    case BC_CLOSURE:
      param = get_param(vm);
      hClosure c = sbClosure_create(param);
      v = pop_stack(vm); /* function to close with */
      if (v->type <= 0) {
        CHECK("internal violation: a function is required to create a closure!");
      }
      while (param > 0) {
        param --;
        /* set_ref: the things on the stack should be the reference variables that
         * have previously been being manipulated using the IND instructions */
        w = peek_stack(vm, 0);
        if (w->type != IT_REF) {
          CHECK("internal violation: closure should receive a set of reference variables (got %lld)", (long long)w->type);
        }
        sbClosure_set_var(c, param, sbVar_deref(w));
        pop_stack(vm);
      }
      v->closure = c;
      push_stack_immediate(vm, v);
      break;
    case BC_LIST_GATHER:
      v = pop_stack(vm);
      if (v->type != IT_INTEGER) {
        /* internal error! this should be generated correctly */
        CHECK("internal violation: LIST_GATHER should receive an integer on top of stack");
      }
      param = v->integer;
      count = 0;
      res = sbV_empty_list(param);
      while (count < param) {
        /* list gets built from top of stack down */
        w = pop_stack(vm);
        sbV_append(&res, w);
        count ++;
      }
      push_stack_immediate(vm, &res);
      break;
    case BC_HASH_GATHER:
      v = pop_stack(vm);
      if (v->type != IT_INTEGER) {
        /* internal error! this should be generated correctly */
        CHECK("internal violation: HASH_GATHER should receive an integer on top of stack");
      }
      count = v->integer;
      res = sbV_empty_hash(count * 3 / 2);
      while (count > 0) {
        /* values come first, then keys, because of execution order */
        x = pop_stack(vm);
        w = pop_stack(vm);
        sbV_index_set(&res, w, x);
        count --;
      }
      push_stack_immediate(vm, &res);
      break;
    case BC_LIST_SPILL:
      v = pop_stack(vm); /* list to spill */
      if (v->type != IT_LIST) {
        /* user error */
        PANIC("cannot use '...' operator on something that isn't a list");
      }
      res = *pop_stack(vm); /* current count */
      if (res.type != IT_INTEGER) {
        /* internal error! this should be generated correctly */
        CHECK("internal violation: LIST_SPILL should receive an integer on top of stack");
      }
      vars = sbList_get_value(v->list, &count);
      for (usize i = count - 1; ; i--) {
        push_stack(vm, &vars[i].value);
        if (i == 0) break;
      }
      res.integer += count;
      push_stack_immediate(vm, &res);
      break;
    case BC_LONG_NUM:
    case BC_VLONG_NUM:
      PANIC("illegal opcode $%02X at position $%016zX", op, (usize)vm->ip);
    default:
      PANIC("unrecognized opcode $%02X at position $%016zX", op, (usize)vm->ip);
  }
  if (vm->debugmode) debug("\n");
}
