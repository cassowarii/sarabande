#include "data/hashtable.h"

#include "data/string.h"

#define HASHES_PER_BLOCK 256
#define INLINE_TABLE_LENGTH 64

/* TODO eliminate globals */
sbBuffer g_hashtable_blocks = {0};

typedef struct hashentry {
  hV key;
  hV value;
} hashentry;

typedef struct hashtbl {
  usize size;
  usize used;
  hHash handle;
  flag allocated;
  flag is_inline;
  union {
    hashentry inline_entries[INLINE_TABLE_LENGTH];
    hashentry *external_entries;
  };
} hashtbl;

typedef struct hashblk {
  usize id;
  usize used_count;
  usize last_index;
  hashtbl tables[HASHES_PER_BLOCK];
} hashblk;

static hashblk *alloc_new_block();
static hashtbl *new_tbl(usize initial_size);
static hashentry *get_entry_ptr_for_tbl(hashtbl *t, usize *length_out);
static hashtbl *find_tbl_for_handle(hHash handle);
static hashentry *set_key(hashtbl *t, hV *key, hV *value);
static hashentry delete_key(hashtbl *t, hV *key);
static void set_hashtbl_size(hashtbl *t, usize new_size, flag rehash_all);
static hashentry *set_key_in_array(hashentry *entries, usize length, hV *key, hV *value);
static hashentry *find_entry_by_key(hashtbl *t, hV *key);

void sbHash_sys_init() {
  sbBuffer_initialize(&g_hashtable_blocks, 8 * sizeof(hashblk*));
  alloc_new_block();
}

void sbHash_sys_deinit() {
  //sbBuffer_deinitialize(&g_hashtable_blocks);
}

/* https://en.wikipedia.org/wiki/Fowler%E2%80%93Noll%E2%80%93Vo_hash_function */
#define FNV_OFFSET_BASIS 0xcbf29ce484222325ULL
#define FNV_PRIME 0x100000001b3ULL
sbHashValue sbHash_hash_bytes(const char *bytes, usize length) {
  sbHashValue result = FNV_OFFSET_BASIS;
  for (usize i = 0; i < length; i++) {
    result ^= bytes[i];
    result *= FNV_PRIME;
  }
  return result;
}

sbHashValue sbHash_hash_obj(hV *obj) {
  if (obj->type == IT_NIL) {
    char zero = 0;
    return sbHash_hash_bytes(&zero, 1);
  } else if (obj->type == IT_STRING) {
    char scratch[8];
    usize length;
    const char *strval = sbString_get_value(obj->string, scratch, &length);
    return sbHash_hash_bytes(strval, length);
  } else {
    PANIC("don't know how to hash object of type %lld", obj->type);
  }
}

hHash sbHash_create(usize initial_size) {
  hashtbl *t = new_tbl(initial_size);
  return t->handle;
}

void sbHash_insert(hHash h, hV *key, hV *value) {
  hashtbl *t = find_tbl_for_handle(h);
  set_key(t, key, value);
}

hV sbHash_find(hHash h, hV *key) {
  hashtbl *t = find_tbl_for_handle(h);
  hashentry *e = find_entry_by_key(t, key);
  if (e->key.type == IT_NOTHING) {
    return (hV) { .type = IT_NOTHING };
  } else {
    return e->value;
  }
}

hV sbHash_find_or_insert(hHash h, hV *key, hV *value) {
  hashtbl *t = find_tbl_for_handle(h);
  hashentry *e = find_entry_by_key(t, key);
  if (e->key.type == IT_NOTHING) {
    e->value = *value;
  }
  return e->value;
}

void sbHash_delete(hHash h, hV *key, hV *value) {
  hashtbl *t = find_tbl_for_handle(h);
  delete_key(t, key);
}

/* --- */

static hashblk *alloc_new_block() {
  usize nblocks = g_hashtable_blocks.size / sizeof(hashblk*);
  hashblk *new_block = calloc(1, sizeof(hashblk));
  new_block->id = nblocks;
  sbBuffer_append(&g_hashtable_blocks, &new_block, sizeof(hashblk*));
  return new_block;
}

static hashblk *find_free_block() {
  usize nblocks = g_hashtable_blocks.size / sizeof(hashblk*);
  hashblk *free_block = NULL;
  for (usize i = 0; i < nblocks; i++) {
    if (((hashblk**)g_hashtable_blocks.data)[i]->used_count < HASHES_PER_BLOCK) {
      free_block = ((hashblk**)g_hashtable_blocks.data)[i];
      break;
    }
  }

  if (free_block == NULL) {
    free_block = alloc_new_block();
  }

  return free_block;
}

static hashblk *get_block(usize index) {
  usize nblocks = g_hashtable_blocks.size / sizeof(hashblk*);
  if (index > nblocks) PANIC("request for a hash block (#%zu) that does not exist!", index);
  return ((hashblk**)g_hashtable_blocks.data)[index];
}

static hashtbl *find_free_entry(hashblk *blk) {
  if (blk->used_count >= HASHES_PER_BLOCK) PANIC("request for new table in full hashblk!");

  while (blk->tables[blk->last_index].allocated) {
    blk->last_index++;
    blk->last_index %= HASHES_PER_BLOCK;
  }
  blk->tables[blk->last_index].allocated = 1;
  blk->used_count ++;

  hashtbl *result = &blk->tables[blk->last_index];

  result->handle = blk->id * HASHES_PER_BLOCK + blk->last_index;

  return result;
}

static void set_hashtbl_size(hashtbl *t, usize new_size, flag rehash_all) {
  if (new_size < INLINE_TABLE_LENGTH) new_size = INLINE_TABLE_LENGTH;
  if (new_size < t->size) PANIC("cannot shrink hashtable allocation");

  if (new_size == INLINE_TABLE_LENGTH) {
    t->is_inline = 1;
  } else {
    hashentry *new_data = calloc(new_size, sizeof(hashentry));
    if (rehash_all) {
      hashentry *current_data = get_entry_ptr_for_tbl(t, NULL);
      for (usize i = 0; i < t->size; i++) {
        if (current_data[i].key.type != IT_NOTHING && current_data[i].key.type != ITX_TOMBSTONE) {
          set_key_in_array(new_data, new_size, &current_data[i].key, &current_data[i].value);
          /* adding a new k/v pair will retain the values, so we need to release them
           * from the previous allocation */
          sbV_release(&current_data[i].key);
          sbV_release(&current_data[i].value);
        }
      }
      /* now that we've migrated all our things to the new version, we can free the old
       * version (unless it was not alloc'd to begin with) */
      if (!t->is_inline) free(current_data);
    }
    t->external_entries = new_data;
    t->is_inline = 0;
  }
  t->size = new_size;
}

static hashentry *get_entry_ptr_for_tbl(hashtbl *t, usize *length_out) {
  if (length_out) *length_out = t->size;
  if (t->is_inline) {
    return t->inline_entries;
  } else {
    return t->external_entries;
  }
}

static hashentry *find_entry_in_array(hashentry *entries, usize length, hV *key) {
  sbHashValue hash = sbHash_hash_obj(key);
  u32 start = (hash & 0xFFFFFFFF);
  u32 move = (hash >> 32);
  while (length % move == 0) {
    move ++;
  }
  usize index = start % length;
  while (!sbV_eq(&entries[index].key, key) && entries[index].key.type != IT_NOTHING) {
    index += move;
    index %= length;
  }
  return &entries[index];
}

static hashentry *find_entry_by_key(hashtbl *t, hV *key) {
  return find_entry_in_array(get_entry_ptr_for_tbl(t, NULL), t->size, key);
}

static hashentry *set_key_in_array(hashentry *entries, usize length, hV *key, hV *value) {
  hashentry *location = find_entry_in_array(entries, length, key);

  /* now, we either found the current entry for this key,
   * or we found an empty slot that fits this key. */
  if (location->key.type == IT_NOTHING) {
    /* key was not here before, so we need to retain the key
     * as well so it doesn't change out from under us */
    sbV_retain(key);
    location->key = *key;
  } else {
    /* replacing something that already exists. we can keep
     * the key the same, but need to release the previous value. */
    sbV_release(&location->value);
  }

  sbV_retain(value);
  location->value = *value;

  return location;
}

static hashentry *set_key(hashtbl *t, hV *key, hV *value) {
  hashentry *location = find_entry_by_key(t, key);

  /* now, we either found the current entry for this key,
   * or we found an empty slot that fits this key. */
  if (location->key.type == IT_NOTHING) {
    /* key was not here before, so we need to retain the key
     * as well so it doesn't change out from under us */
    sbV_retain(key);
    location->key = *key;
    t->used ++;
    if (t->used >= t->size * 3 / 4) {
      set_hashtbl_size(t, t->size * 2, TRUE);
    }
  } else {
    /* replacing something that already exists. we can keep
     * the key the same, but need to release the previous value. */
    sbV_release(&location->value);
  }

  sbV_retain(value);
  location->value = *value;

  return location;
}

static hashentry delete_key(hashtbl *t, hV *key) {
  hashentry *location = find_entry_by_key(t, key);
  hashentry to_return = *location;
  location->key.type = ITX_TOMBSTONE;
  return to_return;
}

static hashtbl *find_tbl_for_handle(hHash handle) {
  hashblk *block = get_block(handle / HASHES_PER_BLOCK);
  return &block->tables[handle % HASHES_PER_BLOCK];
}

static hashtbl *new_tbl(usize initial_size) {
  hashtbl *t = find_free_entry(find_free_block());
  set_hashtbl_size(t, initial_size, FALSE);
  return t;
}
