#ifndef __SARABANDE_BYTECODE_H__
#define __SARABANDE_BYTECODE_H__

#include "global.h"

typedef enum sbOpcode {
  BC_NOP        = 0x00, // skip me
  BC_LD_IMM,            // push immediate value to stack
  BC_LD_CONST,          // push constants[index] to stack
  BC_LD_CTX,            // look up key in context object and push result
  BC_LD_VAR,            // push value onto stack from variable
  BC_LD_UPVAL,          // push value onto stack from closure
  BC_ST_VAR,            // pop value from stack into variable
  BC_ST_UPVAL,          // pop value from stack into closure
  BC_POP        = 0x08, // pop value from stack
  BC_NPOP,              // pop value from stack
  BC_CALL,              // push return address and jump to new block
  BC_RET,               // return to calling block
  BC_JMP,               // unconditional local jump inside current block
  BC_JT,                // jump if true
  BC_JF,                // jump if false
  BC_SEND,              // like call, but jump to object method
  BC_OP_EQ      = 0x10, // ==
  BC_OP_NOT,            // boolean not
  BC_OP_ADD,            // add
  BC_OP_SUB,            // subtract
  BC_OP_MUL,            // multiply
  BC_OP_DIV,            // true divide
  BC_OP_FLDIV,          // floor divide
  BC_OP_NEG,            // negative
  BC_OP_MOD     = 0x18, // mod
  BC_OP_POW,            // power
  BC_OP_INCR,           // increment
  BC_OP_DECR,           // decrement
  BC_OP_DEREF,          // dereference pointer
  BC_ALLOC_VARS,        // create space in rstack for local variables
  BC_LIST_GATHER,       // create list from count + list of values on stack
  BC_HASH_GATHER,       // create hash from count + list of pairs of keys/values on stack
  BC_LONG_NUM   = 0xFC, // next two bytes are a 16-bit number
  BC_VLONG_NUM  = 0xFD, // next four bytes are a 32-bit number
  BC_VVLONG_NUM = 0xFE, // next four bytes are a 32-bit number
  BC_HALT       = 0xFF, // terminate execution
} sbOpcode;

#endif
