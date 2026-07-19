#include "vm/block.h"

#define INITIAL_PROGRAM_BLOCK_SIZE 32

sbVmCompiler sbVmCompiler_create(usize initial_bytecode_size, usize initial_constant_size, flag debugmode) {
  sbVmCompiler cm = {0};
  cm.debugmode = debugmode;
  sbBuffer_initialize(&cm.bytecode, initial_bytecode_size);
  sbBuffer_initialize(&cm.constants, initial_constant_size);
  sbBuffer_initialize(&cm.label_positions, 1024);
  return cm;
}

void sbVmCompiler_reset(sbVmCompiler *cm) {
  sbBuffer_reset(&cm->bytecode);
  sbBuffer_reset(&cm->constants);
}

void sbVmCompiler_deinitialize(sbVmCompiler *cm) {
  sbBuffer_deinitialize(&cm->bytecode);
  sbBuffer_deinitialize(&cm->constants);
  sbBuffer_deinitialize(&cm->label_positions);
}

void sbVmCompiler_write_code(sbVmCompiler *cm, const u32 *data, usize length) {
  sbBuffer_append(&cm->bytecode, data, length * sizeof(u32));
}

void sbVmCompiler_overwrite_code_at(sbVmCompiler *cm, usize offset, const u32 *data, usize length) {
  if (offset + length > cm->bytecode.size) PANIC("buffer overflow with overwrite_code_at!");
  memcpy(&((u32*)cm->bytecode.data)[offset], data, length * sizeof(u32));
}

usize sbVmCompiler_get_position(sbVmCompiler *cm) {
  return cm->bytecode.size / sizeof(u32);
}

u32 sbVmCompiler_add_constant(sbVmCompiler *cm, hVal *constant) {
  sbV_retain(constant);
  sbBuffer_append(&cm->constants, constant, sizeof(hVal));
  return cm->constants.size / sizeof(hVal) - 1;
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
sbBlockId sbVmProgram_add_block(sbVmProgram *pm, sbVmCompiler *cm) {
  usize bytecode_length = cm->bytecode.size;
  usize constants_length = cm->constants.size;

  /* padding to align constants on 8 byte boundary */
  while (bytecode_length % 8 != 0) bytecode_length ++;

  u8 *block_data = sbArena_alloc(&pm->arena, bytecode_length + constants_length);

  u32 *bytecode = (u32*)&block_data[0];
  memcpy(bytecode, cm->bytecode.data, cm->bytecode.size);

  u8 *constants = &block_data[bytecode_length];
  memcpy(constants, cm->constants.data, constants_length);

  sbVmBlock bk = {
    .bytecode = bytecode,
    .bytecode_end = bytecode + bytecode_length,
    .constants = (hVal*)constants,
    .constants_count = constants_length / sizeof(hVal),
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
