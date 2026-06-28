#include "common.h"

#define TEST_VM_BLK(bc, cn) ((sbVmBlock) { \
    .bytecode = bc, \
    .bytecode_length = sizeof(bc), \
    .constants = cn, \
    .constants_count = sizeof(cn)/sizeof(hV) \
  })

/* dynamic sized block that we can more easily add
 * things to while compiling */
typedef struct sbVmPartialBlock {
  sbBuffer bytecode;
  sbBuffer constants;
} sbVmPartialBlock;

/* fixed size / frozen block of bytecode */
typedef struct sbVmBlock {
  const u8 *bytecode;
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

sbVmPartialBlock sbVmBlock_create(usize initial_bytecode_size, usize initial_constant_size);

void sbVmBlock_reset(sbVmPartialBlock *pb);

void sbVmBlock_deinitialize(sbVmPartialBlock *pb);

void sbVmBlock_write_code(sbVmPartialBlock *pb, const u8 *data, usize length);

void sbVmBlock_add_constant(sbVmPartialBlock *pb, hV *constant);

void sbVmProgram_initialize(sbVmProgram *pm, usize initial_arena_size);

void sbVmProgram_deinitialize(sbVmProgram *pm);

/* finalize a partial-block and add it to the program.
 * copies the data, so the same partial-block object can be reset
 * and reused after calling this function. */
sbBlockId sbVmProgram_add_block(sbVmProgram *pm, sbVmPartialBlock *pb);
