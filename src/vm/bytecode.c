#include "vm/bytecode.h"

const char *g_opcode_names[] = {
#define X(A) #A,
  BYTECODES
#undef X
};
