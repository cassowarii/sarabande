#ifndef __SARABANDE_POOL_H__
#define __SARABANDE_POOL_H__

#include "common.h"

typedef struct sbPool {
  usize elem_size;
  usize block_size;
  usize num_blocks;
  usize block_ptr_capacity;
  usize next_block_id;
  void **block_ptrs;
  u16 *used_counts;
} sbPool;

typedef sbPool *hPool;

void sbPool_initialize(hPool pl, usize elem_size, usize block_size);

void sbPool_deinitialize(hPool pl);

void *sbPool_alloc(hPool pl, usize *id_out);

void *sbPool_get_entry(hPool pl, usize index);

#endif
