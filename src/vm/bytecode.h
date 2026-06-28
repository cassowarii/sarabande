#ifndef __SARABANDE_BYTECODE_H__
#define __SARABANDE_BYTECODE_H__

#include "global.h"

typedef enum sbOpcode {
  BC_NOP = 0,
  BC_LD_CONST,
  BC_LD_IMM,
  BC_LD_VAR,
  BC_LD_CTX,
  BC_LD_UPVAL,
  BC_ST_VAR,
  BC_ST_UPVAL,
  BC_CALL,
  BC_JMP,
  BC_RET = 10,
  BC_SEND,
  BC_OP_ADD,
  BC_OP_SUB,
  BC_OP_MUL,
  BC_OP_DIV,
  BC_OP_NEG,
  BC_OP_MOD,
  BC_OP_POW,
  BC_OP_DEREF,
  BC_LIST_NEW = 20,
  BC_HASH_NEW,
  BC_LIST_ADD,
  BC_HASH_ADD,
  BC_LONG_NUM = 253,
  BC_VLONG_NUM = 254,
  BC_HALT = 255,
} sbOpcode;

#endif
