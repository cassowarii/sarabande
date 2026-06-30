#ifndef __SARABANDE_BLOCK_H__
#define __SARABANDE_BLOCK_H__

#include "common.h"

#define TEST_VM_BLK(bc, cn) ((sbVmBlock) { \
    .bytecode = bc, \
    .bytecode_length = sizeof(bc), \
    .constants = cn, \
    .constants_count = sizeof(cn)/sizeof(hV) \
  })

/* dynamic sized block that we can more easily add
 * things to while compiling */
typedef struct sbVmCompiler {
  sbBuffer bytecode;
  sbBuffer constants;
  sbBuffer label_positions;
} sbVmCompiler;

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

sbVmCompiler sbVmCompiler_create(usize initial_bytecode_size, usize initial_constant_size);

void sbVmCompiler_reset(sbVmCompiler *pb);

void sbVmCompiler_deinitialize(sbVmCompiler *pb);

void sbVmCompiler_write_code(sbVmCompiler *pb, const u8 *data, usize length);

void sbVmCompiler_overwrite_code_at(sbVmCompiler *cm, usize offset, const u8 *data, usize length);

usize sbVmCompiler_get_position(sbVmCompiler *cm);

void sbVmCompiler_add_constant(sbVmCompiler *pb, hV *constant);

void sbVmProgram_initialize(sbVmProgram *pm, usize initial_arena_size);

void sbVmProgram_deinitialize(sbVmProgram *pm);

/* finalize a partial-block and add it to the program.
 * copies the data, so the same partial-block object can be reset
 * and reused after calling this function. */
sbBlockId sbVmProgram_add_block(sbVmProgram *pm, sbVmCompiler *pb);

#endif
