#ifndef __SARABANDE_BYTECODE_H__
#define __SARABANDE_BYTECODE_H__

#include "global.h"

#define BYTECODES \
  /* skip me */ \
  X(BC_NOP)\
  /* push immediate value to stack */ \
  X(BC_LD_IMM)\
  /* push constants[index] to stack */ \
  X(BC_LD_CONST)\
  /* look up key in context object and push result */ \
  X(BC_LD_CTX)\
  /* push value onto stack from variable */ \
  X(BC_LD_VAR)\
  /* push lref to variable onto stack */ \
  X(BC_LD_LREF)\
  /* push rref to variable onto stack */ \
  X(BC_LD_RREF)\
  /* push value onto stack from closure */ \
  X(BC_LD_UPVAL)\
  /* push pointer to closure value onto stack (to close over again) */ \
  X(BC_LD_UPREF)\
  /* push reference to function onto stack */ \
  X(BC_LD_BLK)\
  /* push nil onto stack */ \
  X(BC_LD_NIL)\
  /* push true onto stack */ \
  X(BC_LD_TRUE)\
  /* push false onto stack */ \
  X(BC_LD_FALSE)\
  /* pop value from stack into variable */ \
  X(BC_ST_VAR)\
  /* pop value from stack into closure */ \
  X(BC_ST_UPVAL)\
  /* decrement TOS and store NOS in variable */ \
  X(BC_ST_ARG)\
  /* pop value from stack */ \
  X(BC_POP)\
  /* pop N values from stack given by top number */ \
  X(BC_NPOP)\
  /* swap top two values on stack */ \
  X(BC_SWAP)\
  /* push return address and jump to new block */ \
  X(BC_CALL)\
  /* 'a->(...)' indirect call that can return a value */ \
  X(BC_CALL_IND)\
  /* 'a.b' call that should return a reference */ \
  X(BC_DOT)\
  /* 'a.b' indirect call that should return a reference */ \
  X(BC_DOT_IND)\
  /* return to calling block */ \
  X(BC_RET)\
  /* check number of function arguments = this */ \
  X(BC_NUMARG)\
  /* check number of function arguments > this */ \
  X(BC_MINARG)\
  /* unconditional local jump inside current block */ \
  X(BC_JMP)\
  /* jump if true */ \
  X(BC_JT)\
  /* jump if false */ \
  X(BC_JF)\
  /* like call)\ but jump to object method */ \
  X(BC_SEND)\
  /* assign through pointer */ \
  X(BC_REF_PUT)\
  /* == */ \
  X(BC_OP_EQ)\
  /* boolean not */ \
  X(BC_OP_NOT)\
  /* add */ \
  X(BC_OP_ADD)\
  /* subtract */ \
  X(BC_OP_SUB)\
  /* multiply */ \
  X(BC_OP_MUL)\
  /* true divide */ \
  X(BC_OP_DIV)\
  /* floor divide */ \
  X(BC_OP_FLDIV)\
  /* negative */ \
  X(BC_OP_NEG)\
  /* mod */ \
  X(BC_OP_MOD)\
  /* power */ \
  X(BC_OP_POW)\
  /* increment */ \
  X(BC_OP_INCR)\
  /* decrement */ \
  X(BC_OP_DECR)\
  /* dereference pointer */ \
  X(BC_OP_DEREF)\
  /* less than */ \
  X(BC_OP_LT)\
  /* less than or equal to */ \
  X(BC_OP_LE)\
  /* logical and */ \
  X(BC_OP_AND)\
  /* logical or */ \
  X(BC_OP_OR)\
  /* a[...] */ \
  X(BC_OP_INDEXVAL)\
  /* a[...] = ... */ \
  X(BC_OP_INDEXLREF)\
  /* a[...] = ... */ \
  X(BC_OP_INDEXLREF_IND)\
  /* &a[...] */ \
  X(BC_OP_INDEXRREF)\
  /* &a[...] */ \
  X(BC_OP_INDEXRREF_IND)\
  /* index [a..b] */ \
  X(BC_OP_RANGEINDEX)\
  /* create space in rstack for local variables */ \
  X(BC_ALLOC_VARS)\
  /* create closure from top elements of stack */ \
  X(BC_CLOSURE)\
  /* create list from count + list of values on stack */ \
  X(BC_LIST_GATHER)\
  /* create hash from count + list of pairs of keys/values on stack */ \
  X(BC_HASH_GATHER)\
  /* splat a list into a context with a count */ \
  X(BC_LIST_SPILL)\
  /* next two bytes are a 16-bit number */ \
  X(BC_LONG_NUM)\
  /* next four bytes are a 32-bit number */ \
  X(BC_VLONG_NUM)\
  /* next four bytes are a 32-bit number */ \
  X(BC_VVLONG_NUM)\
  /* terminate execution */ \
  X(BC_HALT)

typedef enum sbOpcode {
#define X(A) A,
  BYTECODES
#undef X
} sbOpcode;

extern const char *g_opcode_names[];

#endif
