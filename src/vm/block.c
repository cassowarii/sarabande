#include "vm/block.h"

#define INITIAL_PROGRAM_BLOCK_SIZE 32

sbVmPartialBlock sbVmBlock_create(usize initial_bytecode_size, usize initial_constant_size) {
  sbVmPartialBlock pb = {0};
  sbBuffer_initialize(&pb.bytecode, initial_bytecode_size);
  sbBuffer_initialize(&pb.constants, initial_constant_size);
  return pb;
}

void sbVmBlock_reset(sbVmPartialBlock *pb) {
  sbBuffer_reset(&pb->bytecode);
  sbBuffer_reset(&pb->constants);
}

void sbVmBlock_deinitialize(sbVmPartialBlock *pb) {
  sbBuffer_deinitialize(&pb->bytecode);
  sbBuffer_deinitialize(&pb->constants);
}

void sbVmBlock_write_code(sbVmPartialBlock *pb, const u8 *data, usize length) {
  sbBuffer_append(&pb->bytecode, data, length);
}

void sbVmBlock_add_constant(sbVmPartialBlock *pb, hV *constant) {
  sbV_retain(constant);

  sbBuffer_append(&pb->constants, constant, sizeof(hV));
}

void sbVmProgram_initialize(sbVmProgram *pm, usize initial_arena_size) {
  *pm = (sbVmProgram) {0};
  sbArena_initialize(&pm->arena, initial_arena_size);
  pm->block_capacity = INITIAL_PROGRAM_BLOCK_SIZE;
  pm->blocks = malloc(pm->block_capacity * sizeof(sbVmBlock));
}

void sbVmProgram_deinitialize(sbVmProgram *pm) {
  free(pm->blocks);
  sbArena_deinitialize(&pm->arena);
  *pm = (sbVmProgram) {0};
}

/* finalize a partial-block and add it to the program.
 * copies the data, so the same partial-block object can be reset
 * and reused after calling this function. */
sbBlockId sbVmProgram_add_block(sbVmProgram *pm, sbVmPartialBlock *pb) {
  usize bytecode_length = pb->bytecode.size;
  usize constants_length = pb->constants.size;
  while (bytecode_length % 8 != 0) bytecode_length ++;

  u8 *block_data = sbArena_alloc(&pm->arena, bytecode_length + constants_length);

  u8 *bytecode = &block_data[0];
  memcpy(bytecode, pb->bytecode.data, bytecode_length);

  u8 *constants = &block_data[bytecode_length];
  memcpy(constants, pb->constants.data, constants_length);

  sbVmBlock bk = {
    .bytecode = bytecode,
    .bytecode_length = bytecode_length,
    .constants = (hV*)constants,
    .constants_count = constants_length / sizeof(hV),
  };

  if (pm->block_count >= pm->block_capacity) {
    usize new_capacity = pm->block_capacity * 3 / 2;
    if (new_capacity <= pm->block_capacity) new_capacity = pm->block_capacity + 1;
    void *new_blocks = realloc(pm->blocks, new_capacity * sizeof(sbVmBlock));
    if (new_blocks) {
      pm->blocks = new_blocks;
      pm->block_capacity = new_capacity;
    } else {
      PANIC("ran out of memory while compiling program!");
    }
  }

  sbBlockId result = pm->block_count;

  pm->blocks[pm->block_count] = bk;

  pm->block_count ++;

  return result;
}
