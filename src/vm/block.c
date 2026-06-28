#include "vm/block.h"

#define INITIAL_PROGRAM_BLOCK_SIZE 32

void sbVmProgram_init(sbVmProgram *pm, usize initial_arena_size) {
  *pm = (sbVmProgram) {0};
  sbArena_initialize(&pm->arena, initial_arena_size);
}

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

void sbVmProgram_initialize(sbVmProgram *pm) {
  pm->block_count = 0;
  pm->block_capacity = INITIAL_PROGRAM_BLOCK_SIZE;
  pm->blocks = malloc(pm->block_capacity * sizeof(sbVmBlock));
}

void sbVmProgram_deinitialize(sbVmProgram *pm) {
  free(pm->blocks);
  *pm = (sbVmProgram) {0};
}

sbBlockId sbVmProgram_add_block(sbVmProgram *pm, sbVmPartialBlock *pb) {
  const usize bytecode_length = pb->bytecode.size;
  char *bytecode = sbArena_alloc(&pm->arena, bytecode_length);
  memcpy(bytecode, pb->bytecode.data, bytecode_length);

  const usize constants_length = pb->constants.size;
  char *constants = sbArena_alloc(&pm->arena, constants_length);
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
