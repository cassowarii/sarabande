#include "common.h"

/* dynamic sized block that we can more easily add
 * things to while compiling */
typedef struct sbVmPartialBlock {
  sbBuffer bytecode;
  sbBuffer constants;
} sbVmPartialBlock;

/* fixed size / frozen block of bytecode */
typedef struct sbVmBlock {
  const char *bytecode;
  usize bytecode_length;
  const hV *constants;
  usize constants_count;
} sbVmBlock;

typedef struct sbVmProgram {
  sbArena arena;
  sbVmBlock *blocks;
  usize block_count;
  usize block_capacity;
} sbVmProgram;

typedef usize sbBlockId;

void sbVmProgram_init(sbVmProgram *pm, usize initial_arena_size);

sbVmPartialBlock sbVmBlock_create(usize initial_bytecode_size, usize initial_constant_size);

void sbVmBlock_reset(sbVmPartialBlock *pb);

void sbVmBlock_deinitialize(sbVmPartialBlock *pb);

void sbVmProgram_initialize(sbVmProgram *pm);

void sbVmProgram_deinitialize(sbVmProgram *pm);

sbBlockId sbVmProgram_add_block(sbVmProgram *pm, sbVmPartialBlock *pb);
