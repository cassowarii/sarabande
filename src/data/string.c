#include "data/string.h"

#include "mem/mem.h"
#include <string.h>

#define INLINE_BUFFER_SIZE 256
#define STRINGS_PER_BLOCK 256

/* highest bit set on handle value means it is a tinystr */
#define FLAG_TINY (1ULL << 63)

/* if string length < REQ_ALLOC_LENGTH, we can just put the
 * string in the handle itself. this should be 64 / 8 = 8,
 * we need to allocate for an 8 char string because tinystr
 * uses one byte to store the string length as well. */
#define REQ_ALLOC_LENGTH (sizeof(hString)/sizeof(char))

/* TODO: Move these globals into some kind of "execution context" struct
 * that we can pass around to be reentrant */
/* also TODO a better (slightly more annoying to code) way to structure
 * this would be SoA style: instead of storing used_count and allocated
 * inside the block and entry respectively, store them in a separate
 * array next to the pointers that we can scan quickly. less important
 * for allocated since it's already in the strblk anyway, but for used_count
 * this could be important because the counts will otherwise be scattered
 * to the four winds */
sbBuffer g_string_blocks;
usize g_free_block_index;

typedef struct strentry {
  usize length;
  hString handle;
  flag is_external;
  flag allocated;
  union {
    char inline_value[INLINE_BUFFER_SIZE];
    sbBuffer somewhere_else_value;
  };
} strentry;

typedef struct strblk {
  usize id;
  usize used_count;
  usize next_free_index;
  strentry entries[STRINGS_PER_BLOCK];
} strblk;

static const char *get_ptr_of_entry(strentry *e);
static strblk *get_strblk(usize index);
static strentry *get_entry(strblk *blk, usize index);
static strentry *new_entry(usize length);
static void place_str_at_offset(strentry *e, usize offset, const char *str, usize length);
static flag is_tinystr(hString handle);
static strblk *new_strblk();

void sbString_sys_init() {
  g_free_block_index = 0;
  sbBuffer_initialize(&g_string_blocks, sizeof(strblk*) * 8);

  /* we need to have at least one strblk to start */
  new_strblk();
}

void sbString_sys_deinit() {
  /* TODO free these i guess, or don't deinitialize this buffer, right now this
   * would just drop the pointers to no benefit. look, i'm going to write a
   * garbage collector, ok? give me a second */
  //sbBuffer_deinitialize(&g_string_blocks);
}

hString sbString_new(const char *value, usize length) {
  if (length >= REQ_ALLOC_LENGTH) {
    strentry *e = new_entry(length);
    place_str_at_offset(e, 0, value, length);
    return e->handle;
  } else {
    /* tinystr */
    hString new_handle = 0;
    for (int i = 0; i < length; i++) {
      new_handle <<= 8;
      new_handle |= value[i];
    }
    /* set length in high bit */
    new_handle |= ((length << (8 * 7)) | FLAG_TINY);
    return new_handle;
  }
}

const char *sbString_get_value(hString handle, char *buffer_i_might_use, usize *length_out) {
  if (is_tinystr(handle)) {
    /* string is stored backwards because it's easiest to just get the low byte */
    usize tinylength = ((handle >> (8 * 7)) & 0x8F);
    if (length_out) *length_out = tinylength;
    if (buffer_i_might_use) {
      usize index = 0;
      hString handle_to_eat = handle;
      while (index < tinylength) {
        buffer_i_might_use[index] = (handle_to_eat & 0xFF); /* get lowest byte and move down */
        handle_to_eat >>= 8;
        index ++;
      }
      buffer_i_might_use[index] = '\0'; /* don't forget NUL! */
    }
    return buffer_i_might_use;
  } else {
    strblk *blk = get_strblk(handle / STRINGS_PER_BLOCK);
    strentry *entry = get_entry(blk, handle % STRINGS_PER_BLOCK);
    if (length_out) *length_out = entry->length;
    return get_ptr_of_entry(entry);
  }
}

usize sbString_get_length(hString handle) {
  if (handle & FLAG_TINY) {
    /* length is top byte except high bit */
    return (handle >> (8 * 7)) & 0x8F;
  } else {
    /* length is just in the entry */
    strblk *blk = get_strblk(handle / STRINGS_PER_BLOCK);
    strentry *entry = get_entry(blk, handle % STRINGS_PER_BLOCK);
    return entry->length;
  }
}

/* --- */

static flag is_tinystr(hString handle) {
  return handle & FLAG_TINY;
}

static const char *get_ptr_of_entry(strentry *e) {
  if (e->is_external) {
    return e->somewhere_else_value.data;
  } else {
    return e->inline_value;
  }
}

static char *get_mutable_ptr_of_entry(strentry *e) {
  /* TODO: When we have static strings, panic here if we try to get
   * a mutable ptr to one of them. (Should copy instead first.) */
  if (e->is_external) {
    return e->somewhere_else_value.data;
  } else {
    return e->inline_value;
  }
}

static strblk *get_strblk(usize index) {
  usize nblocks = g_string_blocks.size / sizeof(strblk*);
  if (index >= nblocks) PANIC("attempt to get a string from a block (#%zu) that does not exist!", index);

  return ((strblk**)g_string_blocks.data)[index];
}

static strentry *get_entry(strblk *blk, usize index) {
  if (index >= STRINGS_PER_BLOCK) PANIC("attempt to get a string (#%zu) that does not exist in block (#%zu)!", index, blk->id);

  return &blk->entries[index];
}

static strblk *new_strblk() {
  usize nblocks = g_string_blocks.size / sizeof(strblk*);
  strblk *new_block = calloc(1, sizeof(strblk));
  new_block->id = nblocks;
  strblk **where_to_put = sbBuffer_expand(&g_string_blocks, sizeof(strblk*));
  *where_to_put = new_block;
  return new_block;
}

static strentry *alloc_entry(strblk *blk) {
  if (blk->used_count == STRINGS_PER_BLOCK) PANIC("attempt to allocate from a full strblk (#%zu)!", blk->id);

  blk->used_count ++;

  usize free_index = blk->next_free_index;

  if (blk->used_count < STRINGS_PER_BLOCK) {
    usize next_free_index = blk->next_free_index;
    do {
      next_free_index ++;
      next_free_index %= STRINGS_PER_BLOCK;
    } while (blk->entries[free_index].allocated && next_free_index != blk->next_free_index);

    if (next_free_index == blk->next_free_index) {
      PANIC("free count lied! for strblk (#%zu)", blk->id);
    }

    blk->next_free_index = next_free_index;
  }

  blk->entries[free_index].allocated = TRUE;

  return &blk->entries[free_index];
}

static strblk *find_free_block() {
  strblk *block_with_free = get_strblk(g_free_block_index);

  if (block_with_free->used_count == STRINGS_PER_BLOCK) {
    usize nblocks = g_string_blocks.size / sizeof(strblk*);
    usize start_index = g_free_block_index;

    do {
      g_free_block_index ++;
      g_free_block_index %= nblocks;
      block_with_free = get_strblk(g_free_block_index);
    } while (g_free_block_index != start_index && block_with_free->used_count == STRINGS_PER_BLOCK);

    if (g_free_block_index == start_index) {
      /* Where did that bring you? Back to me. */
      block_with_free = new_strblk();
      g_free_block_index = nblocks;
    }
  }

  return block_with_free;
}

static void set_entry_size(strentry *e, usize length) {
  if (e->is_external) {
    sbBuffer_set_size(&e->somewhere_else_value, length + 1);
    /* if we shortened string, need to replace NUL at end */
    e->length = length;
    e->somewhere_else_value.data[length] = '\0';
  } else {
    if (length > INLINE_BUFFER_SIZE - 1) {
      /* new length is too big for inline, so we need to copy to the heap */
      sbBuffer new_buf;
      /* length + 1 for NUL terminator. even though we track the length, it's
       * easier to just preserve NUL terminator for talking to C lib functions */
      sbBuffer_initialize(&new_buf, length + 1);
      if (e->length > 0) {
        sbBuffer_append(&new_buf, e->inline_value, e->length + 1);
      }
      e->length = length;
      e->is_external = TRUE;
      e->somewhere_else_value = new_buf;
      e->somewhere_else_value.data[length] = '\0';
    } else {
      /* we can remain inline, but need to set NUL in the right place */
      e->length = length;
      e->inline_value[length] = '\0';
    }
  }
}

static void place_str_at_offset(strentry *e, usize offset, const char *str, usize length) {
  if (offset >= e->length) return; /* no room */

  usize max_bytes_to_write = e->length - offset;
  if (length < max_bytes_to_write) max_bytes_to_write = length;

  char *where_to_put = get_mutable_ptr_of_entry(e) + offset;
  memcpy(where_to_put, str, max_bytes_to_write);
}

static strentry *new_entry(usize length) {
  strblk *new_block = find_free_block();
  strentry *new_entry = alloc_entry(new_block);
  new_entry->handle = new_block->id * STRINGS_PER_BLOCK + (new_entry - new_block->entries) / sizeof(strentry);
  set_entry_size(new_entry, length);
  return new_entry;
}
