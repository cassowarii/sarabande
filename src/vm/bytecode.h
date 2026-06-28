#ifndef __SARABANDE_BYTECODE_H__
#define __SARABANDE_BYTECODE_H__

#include "global.h"

typedef enum sbOpcode {
  BC_NOP = 0,
  BC_LD_IMM,    /* push immediate value to stack */
  BC_LD_CONST,  /* push constants[index] to stack */
  BC_LD_CTX,    /* look up key in context object and push result */
  BC_LD_VAR,    /* push value onto stack from variable */
  BC_LD_UPVAL,  /* push value onto stack from closure */
  BC_ST_VAR,    /* pop value from stack into variable */
  BC_ST_UPVAL,  /* pop value from stack into closure */
  BC_POP,       /* pop value from stack */
  BC_CALL,      /* push return address and jump to new block */
  BC_JMP,       /* local jump inside current block */
  BC_RET,       /* return to calling block */
  BC_SEND,      /* like call, but jump to object method (maybe this is the same as call?) */
  BC_OP_ADD,
  BC_OP_SUB,
  BC_OP_MUL,
  BC_OP_DIV = 0x10,
  BC_OP_FLDIV,
  BC_OP_NEG,
  BC_OP_MOD,
  BC_OP_POW,
  BC_OP_DEREF,
  BC_ALLOC_VARS,
  BC_LIST_NEW,
  BC_HASH_NEW,
  BC_LIST_ADD,
  BC_HASH_ADD,
  BC_LONG_NUM = 253,
  BC_VLONG_NUM = 254,
  BC_HALT = 255,
} sbOpcode;

#endif
