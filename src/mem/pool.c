#include "mem/pool.h"

/* this could be faster: computing offsets at runtime isn't
 * strictly speaking optimal. but this would be insane to macro.
 * "if only c had templates!" <- sentences dreamed up by the
 * utterly deranged.  maybe i should manually code-generate these
 * later. */

// sorry
// BLOCK_ALLOCATION_FLAGS(pool, block_index) gives the array of flags of length `block_size` for the given block
// BLOCK_ALLOCATED_DATA(pool, block_index) gives a pointer to the start of the actual slot array (of size `elem_size * block_size`)
#define BLOCK_ALLOCATION_FLAGS(pool, blk) ((flag*)(((Block*)(pool->block_ptrs[blk]))->data))
#define BLOCK_ALLOCATED_DATA(pool, blk) ((char*)(&((Block*)(pool->block_ptrs[blk]))->data[pool->block_size]))

/*
typedef struct sbPool {
  usize elem_size;
  usize block_size;
  usize num_blocks;
  usize block_ptr_capacity;
  usize next_block_id;
  void **block_ptrs;
  u16 *used_counts;
} sbPool;
*/

typedef struct Block {
  usize last_index;
  char data[];
} Block;

void alloc_new_block(hPool pl);

void sbPool_initialize(hPool pl, usize elem_size, usize block_size) {
  *pl = (sbPool) {0};
  pl->elem_size = elem_size;
  pl->block_size = block_size;
  pl->block_ptr_capacity = 8;
  pl->block_ptrs = calloc(pl->block_ptr_capacity, sizeof(void*));
  pl->used_counts = calloc(pl->block_ptr_capacity, sizeof(u16));
  alloc_new_block(pl);
}

void sbPool_deinitialize(hPool pl) {
  for (usize i = 0; i < pl->num_blocks; i++) {
    free(pl->block_ptrs[i]);
  }
  *pl = (sbPool) {0};
}

void *sbPool_alloc(hPool pl, usize *id_out) {
  usize start_index = pl->next_block_id;
  /* if current block is full, look for one that's empty */
  while (pl->used_counts[pl->next_block_id] == pl->block_size) {
    pl->next_block_id ++;
    pl->next_block_id %= pl->num_blocks;
    if (pl->next_block_id == start_index) {
      /* Where did that bring you? Back to me. */
      pl->next_block_id = pl->num_blocks;
      alloc_new_block(pl);
      break;
    }
  }

  Block *block = pl->block_ptrs[pl->next_block_id];
  while (BLOCK_ALLOCATION_FLAGS(pl, pl->next_block_id)[block->last_index]) {
    block->last_index ++;
    block->last_index %= pl->block_size;
  }
  BLOCK_ALLOCATION_FLAGS(pl, pl->next_block_id)[block->last_index] = 1;
  pl->used_counts[pl->next_block_id] ++;

  if (id_out) *id_out = pl->next_block_id * pl->block_size + block->last_index;

  return &BLOCK_ALLOCATED_DATA(pl, pl->next_block_id)[block->last_index * pl->elem_size];
}

void *sbPool_get_entry(hPool pl, usize index) {
  usize block_id = index / pl->block_size;
  usize elem_id = index % pl->block_size;
  if (!BLOCK_ALLOCATION_FLAGS(pl, block_id)[elem_id]) {
    CHECK("attempt to get unallocated index from pool");
  }
  return &BLOCK_ALLOCATED_DATA(pl, block_id)[elem_id * pl->elem_size];
}

/* this 'alloc' function gives us an unallocated slot. there is no 'free' on individual items
 * (we only have a pointer to the individual item anyway), but items should be reclaimed
 * by the garbage collector. once we write the garbage collector, at least...) */

/* --- */

void alloc_new_block(hPool pl) {
  /* allocate block header + 'block_size' bytes for tracking allocation + enough space
   * to hold all the elements */
  void *new_block = calloc(1, sizeof(Block) + pl->block_size + pl->elem_size * pl->block_size);
  if (pl->num_blocks >= pl->block_ptr_capacity) {
    usize new_capacity = pl->block_ptr_capacity * 2;
    void **new_data = realloc(pl->block_ptrs, new_capacity);
    if (!new_data) {
      PANIC("This should probably trigger the garbage collector or something!");
    }
    pl->block_ptrs = new_data;
  }

  pl->block_ptrs[pl->num_blocks] = new_block;
  pl->num_blocks ++;
}
