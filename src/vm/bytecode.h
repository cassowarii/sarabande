#ifndef __SARABANDE_BYTECODE_H__
#define __SARABANDE_BYTECODE_H__

#include "global.h"

typedef enum sbOpcode {
  BC_NOP        = 0x00, // skip me
  BC_LD_IMM,            // push immediate value to stack
  BC_LD_CONST,          // push constants[index] to stack
  BC_LD_CTX,            // look up key in context object and push result
  BC_LD_VAR,            // push value onto stack from variable
  BC_LD_LREF,           // push lvalue reference onto stack from variable
  BC_LD_RREF,           // push rvalue reference onto stack from variable
  BC_LD_UPVAL,          // push value onto stack from closure
  BC_LD_UPREF,          // push pointer to closure value onto stack (to close over again)
  BC_LD_BLK,            // push reference to function onto stack
  BC_LD_NIL,            // push nil onto stack
  BC_LD_TRUE,           // push true onto stack
  BC_LD_FALSE,          // push false onto stack
  BC_ST_VAR,            // pop value from stack into variable
  BC_ST_UPVAL,          // pop value from stack into closure
  BC_ST_ARG,            // decrement TOS and store NOS in variable
  BC_POP,               // pop value from stack
  BC_NPOP,              // pop N values from stack given by top number
  BC_SWAP,              // swap top two values on stack
  BC_CALL,              // push return address and jump to new block
  BC_RET,               // return to calling block
  BC_NUMARG,            // check number of function arguments = this
  BC_MINARG,            // check number of function arguments > this
  BC_JMP,               // unconditional local jump inside current block
  BC_JT,                // jump if true
  BC_JF,                // jump if false
  BC_SEND,              // like call, but jump to object method
  BC_OP_EQ,             // ==
  BC_OP_NOT,            // boolean not
  BC_OP_ADD,            // add
  BC_OP_SUB,            // subtract
  BC_OP_MUL,            // multiply
  BC_OP_DIV,            // true divide
  BC_OP_FLDIV,          // floor divide
  BC_OP_NEG,            // negative
  BC_OP_MOD,            // mod
  BC_OP_POW,            // power
  BC_OP_INCR,           // increment
  BC_OP_DECR,           // decrement
  BC_OP_DEREF,          // dereference pointer
  BC_OP_LT,             // less than
  BC_OP_LE,             // less than or equal to
  BC_OP_AND,            // logical and
  BC_OP_OR,             // logical or
  BC_OP_INDEX,          // index [] or ::
  BC_OP_RANGEINDEX,     // index [a..b]
  BC_ALLOC_VARS,        // create space in rstack for local variables
  BC_CLOSURE,           // create closure from top elements of stack
  BC_LIST_GATHER,       // create list from count + list of values on stack
  BC_HASH_GATHER,       // create hash from count + list of pairs of keys/values on stack
  BC_LIST_SPILL,        // splat a list into a context with a count
  BC_LONG_NUM   = 0xFC, // next two bytes are a 16-bit number
  BC_VLONG_NUM  = 0xFD, // next four bytes are a 32-bit number
  BC_VVLONG_NUM = 0xFE, // next four bytes are a 32-bit number
  BC_HALT       = 0xFF, // terminate execution
} sbOpcode;

#endif
