#include "integer.h"

/* reserve high bit for sign bit.
 * second-highest bit marks this as a handle to a bigint, which
 * means normally integers have to be less than +/- 2^62. */
#define FLAG_BIGINT (1LL << 62)

#define NUM_PER_BLOCK 256

sbBuffer g_bigint_blocks = {0};

typedef struct bigint {
  flag allocated;
  hInteger handle;
  flag sign_bit;
  sbBuffer buf;
} bigint;

typedef struct intblk {
  usize id;
  usize used_count;
  usize last_index;
  bigint entries[NUM_PER_BLOCK];
} intblk;

flag is_bigint(hInteger n);
static intblk *alloc_new_block();
static intblk *find_free_block();
static intblk *get_block(usize index);
static bigint *find_free_entry(intblk *blk);
static void set_bigint_size(bigint *i, usize new_size);
static bigint *find_int_for_handle(hInteger handle);
static bigint *new_bigint(usize initial_size);

void sbInteger_sys_init() {
  sbBuffer_initialize(&g_bigint_blocks, sizeof(intblk*));
}

hInteger sbInteger_new(u64 value) {
  if (value >= SARABANDE_INT_MIN && value <= SARABANDE_INT_MAX) {
    return value;
  } else {
    bigint *i = new_bigint(2);

    if (value < 0) {
      i->sign_bit = 1;
      value *= -1;
    }
    i->buf.data[0] = (value & 0xFFFFFFFF);
    i->buf.data[1] = ((value >> 32) & 0xFFFFFFFF);

    return i->handle;
  }
}

hInteger sbInteger_sum(hInteger a, hInteger b) {
  if (!is_bigint(a) && !is_bigint(b)) {
    /* if the sum is too big, this will create a bigint. we don't need to
     * worry about signed overflow, because non-bigints are limited to
     * 62 bits of magnitude, so the sum of two can't be bigger than 2^63 */
    return sbInteger_new(a + b);
  }
  PANIC("I haven't implemented this yet!");
}

hInteger sbInteger_diff(hInteger a, hInteger b) {
  if (!is_bigint(a) && !is_bigint(b)) {
    /* if the sum is too big, this will create a bigint. we don't need to
     * worry about signed overflow, because non-bigints are limited to
     * 62 bits of magnitude, so the sum of two can't be bigger than 2^63 */
    return sbInteger_new(a - b);
  }
  if (is_bigint(a)) {
    /* i will use this later */
    bigint *i = find_int_for_handle(a);
    (void)i;
  }
  PANIC("I haven't implemented this yet!");
}

hInteger sbInteger_mul(hInteger a, hInteger b) {
  if (!is_bigint(a) && !is_bigint(b)) {
    if (b < SARABANDE_INT_MAX / a && b > SARABANDE_INT_MIN / a) {
      return sbInteger_new(a * b);
    }
  }
  PANIC("I haven't implemented this yet!");
}

hInteger sbInteger_floordiv(hInteger a, hInteger b) {
  if (!is_bigint(a) && !is_bigint(b)) {
    return sbInteger_new(a / b);
  }
  PANIC("I haven't implemented this yet!");
}

/* --- */

flag is_bigint(hInteger n) {
  return (n & FLAG_BIGINT);
}

static intblk *alloc_new_block() {
  usize nblocks = g_bigint_blocks.size / sizeof(intblk*);
  intblk *new_block = calloc(1, sizeof(intblk));
  new_block->id = nblocks;
  sbBuffer_append(&g_bigint_blocks, &new_block, sizeof(intblk*));
  return new_block;
}

static intblk *find_free_block() {
  usize nblocks = g_bigint_blocks.size / sizeof(intblk*);
  intblk *free_block = NULL;
  for (usize i = 0; i < nblocks; i++) {
    if (((intblk**)g_bigint_blocks.data)[i]->used_count < NUM_PER_BLOCK) {
      free_block = ((intblk**)g_bigint_blocks.data)[i];
      break;
    }
  }

  if (free_block == NULL) {
    free_block = alloc_new_block();
  }

  return free_block;
}

static intblk *get_block(usize index) {
  usize nblocks = g_bigint_blocks.size / sizeof(intblk*);
  if (index > nblocks) PANIC("request for a bigint block (#%zu) that does not exist!", index);
  return ((intblk**)g_bigint_blocks.data)[index];
}

static bigint *find_free_entry(intblk *blk) {
  if (blk->used_count >= NUM_PER_BLOCK) PANIC("request for new entry in full intblk!");

  while (blk->entries[blk->last_index].allocated) {
    blk->last_index++;
    blk->last_index %= NUM_PER_BLOCK;
  }
  blk->entries[blk->last_index].allocated = 1;
  blk->used_count ++;

  bigint *result = &blk->entries[blk->last_index];

  result->handle = ((blk->id * NUM_PER_BLOCK + blk->last_index) | FLAG_BIGINT);

  return result;
}

static void set_bigint_size(bigint *i, usize new_size) {
  sbBuffer_set_size(&i->buf, new_size * sizeof(u32));
}

static bigint *find_int_for_handle(hInteger handle) {
  hInteger h = handle & ~FLAG_BIGINT;
  intblk *block = get_block(h / NUM_PER_BLOCK);
  return &block->entries[h % NUM_PER_BLOCK];
}

static bigint *new_bigint(usize initial_size) {
  bigint *i = find_free_entry(find_free_block());
  set_bigint_size(i, initial_size);
  return i;
}


