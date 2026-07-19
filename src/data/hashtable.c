#include "data/hashtable.h"

#include "data/string.h"

#define HASHES_PER_BLOCK 256
#define INLINE_TABLE_LENGTH 64

/* TODO eliminate globals */
sbPool g_hashtable_pool = {0};

typedef struct hashtbl {
  usize capacity;
  usize used;
  usize n_elems;
  hHash handle;
  flag allocated;
  flag is_inline;
  union {
    struct {
      hVal keys[INLINE_TABLE_LENGTH];
      sbVar values[INLINE_TABLE_LENGTH];
    } internal;
    struct {
      hVal *keys;
      sbVar *values;
    } external;
  };
} hashtbl;

static hashtbl *new_tbl(usize initial_size);
static hVal *get_key_ptr_for_tbl(hashtbl *t, usize *length_out);
static void get_ptrs_for_tbl(hashtbl *t, hVal **keys, sbVar **values);
static hashtbl *find_tbl_for_handle(hHash handle);
static usize set_key(hashtbl *t, hVal *key, hVal *value);
static void delete_key(hashtbl *t, hVal *key);
static hVal delete_key_and_return(hashtbl *t, hVal *key);
static void set_hashtbl_size(hashtbl *t, usize new_size, flag rehash_all);
static usize set_key_for_rehash(hVal *keys, sbVar *values, usize length, hVal *key, sbVar *value);
static usize find_index_by_key(hashtbl *t, hVal *key);

void sbHash_sys_init() {
  sbPool_initialize(&g_hashtable_pool, sizeof(hashtbl), HASHES_PER_BLOCK);
}

void sbHash_sys_deinit() {
  //sbPool_deinitialize(&g_hashtable_pool);
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

flag is_flatly_hashable(hVal *obj) {
  return obj->type == IT_NIL
      || obj->type == IT_BOOLEAN
      || obj->type == IT_INTEGER /* todo: wrong for bigints */
      || obj->type == IT_REF
      || obj->type == IT_SYMBOL;
}

sbHashValue sbHash_hash_obj(hVal *obj) {
  if (is_flatly_hashable(obj)) {
    return sbHash_hash_bytes((char*)obj, sizeof(*obj));
  } else if (obj->type == IT_STRING) {
    char scratch[8];
    usize length;
    const char *strval = sbString_get_value(obj->string, scratch, &length);
    return sbHash_hash_bytes(strval, length);
  } else {
    PANIC("don't know how to hash object of type %lld", (long long)obj->type);
  }
}

hHash sbHash_create(usize initial_size) {
  hashtbl *t = new_tbl(initial_size);
  return t->handle;
}

void sbHash_insert(hHash h, hVal *key, hVal *value) {
  hashtbl *t = find_tbl_for_handle(h);
  set_key(t, key, value);
}

hVal *sbHash_find_value(hHash h, hVal *key) {
  hashtbl *t = find_tbl_for_handle(h);
  usize index = find_index_by_key(t, key);
  hVal *keys;
  sbVar *values;
  get_ptrs_for_tbl(t, &keys, &values);
  if (keys[index].type == IT_NOTHING) {
    return NULL;
  } else {
    return sbVar_get_value_ptr(&values[index]);
  }
}

hVal sbHash_find_lvalue_ref(hHash h, hVal *key) {
  hashtbl *t = find_tbl_for_handle(h);
  usize index = find_index_by_key(t, key);
  hVal *keys;
  sbVar *values;
  get_ptrs_for_tbl(t, &keys, &values);
  return sbVar_get_lvalue_ref(&values[index]);
}

hVal sbHash_find_rvalue_ref(hHash h, hVal *key) {
  hashtbl *t = find_tbl_for_handle(h);
  usize index = find_index_by_key(t, key);
  hVal *keys;
  sbVar *values;
  get_ptrs_for_tbl(t, &keys, &values);
  return sbVar_get_rvalue_ref(&values[index]);
}

/*hVal *sbHash_find_or_insert(hHash h, hVal *key, hVal *value) {
  hashtbl *t = find_tbl_for_handle(h);
  usize index = find_index_by_key(t, key);
  hVal *keys, *values;
  get_ptrs_for_tbl(t, &keys, &values);
  if (keys[index].type == IT_NOTHING) {
    values[index] = *value;
  }
  return &values[index];
}*/

void sbHash_delete(hHash h, hVal *key) {
  hashtbl *t = find_tbl_for_handle(h);
  delete_key(t, key);
}

hVal sbHash_delete_and_return(hHash h, hVal *key) {
  hashtbl *t = find_tbl_for_handle(h);
  return delete_key_and_return(t, key);
}

usize sbHash_get_size(hHash h) {
  hashtbl *t = find_tbl_for_handle(h);
  return t->n_elems;
}

/* --- */

static void set_hashtbl_size(hashtbl *t, usize new_size, flag rehash_all) {
  if (new_size < INLINE_TABLE_LENGTH) new_size = INLINE_TABLE_LENGTH;
  if (new_size < t->capacity) PANIC("cannot shrink hashtable allocation");

  if (new_size == INLINE_TABLE_LENGTH) {
    t->is_inline = 1;
  } else {
    hVal *new_keys = calloc(new_size, sizeof(hVal));
    sbVar *new_values = calloc(new_size, sizeof(sbVar));
    if (rehash_all) {
      hVal *current_keys;
      sbVar *current_values;
      get_ptrs_for_tbl(t, &current_keys, &current_values);
      for (usize i = 0; i < t->capacity; i++) {
        if (current_keys[i].type == IT_NOTHING || current_keys[i].type == ITX_TOMBSTONE) continue;
        set_key_for_rehash(new_keys, new_values, new_size, &current_keys[i], &current_values[i]);
      }
      /* now that we've migrated all our things to the new version, we can free the old
       * version (unless it was not alloc'd to begin with) */
      if (!t->is_inline) {
        free(current_keys);
        free(current_values);
      }
    }
    t->external.keys = new_keys;
    t->external.values = new_values;
    t->is_inline = 0;
  }
  t->capacity = new_size;
}

static hVal *get_key_ptr_for_tbl(hashtbl *t, usize *length_out) {
  if (length_out) *length_out = t->capacity;
  if (t->is_inline) {
    return t->internal.keys;
  } else {
    return t->external.keys;
  }
}

static void get_ptrs_for_tbl(hashtbl *t, hVal **keys, sbVar **values) {
  if (t->is_inline) {
    if (keys) *keys = t->internal.keys;
    if (values) *values = t->internal.values;
  } else {
    if (keys) *keys = t->external.keys;
    if (values) *values = t->external.values;
  }
}

static usize find_key_index_in_array(hVal *keys, usize length, hVal *key) {
  sbHashValue hash = sbHash_hash_obj(key);
  u32 start = (hash & 0xFFFFFFFF);
  u32 move = (hash >> 32);
  while (length % move == 0) {
    move ++;
  }
  usize index = start % length;
  while (!sbV_c_eq(&keys[index], key) && keys[index].type != IT_NOTHING) {
    index += move;
    index %= length;
  }
  return index;
}

static usize find_index_by_key(hashtbl *t, hVal *key) {
  return find_key_index_in_array(get_key_ptr_for_tbl(t, NULL), t->capacity, key);
}

/* NOTE: This is used for rehashing, so it doesn't update reference counts on values.
 * Use set_key for actually retaining refcounts properly. */
static usize set_key_for_rehash(hVal *keys, sbVar *values, usize length, hVal *key, sbVar *value) {
  usize index = find_key_index_in_array(keys, length, key);

  /* now, we either found the current entry for this key,
   * or we found an empty slot that fits this key. */
  if (keys[index].type == IT_NOTHING) {
    /* key was not here before, so we need to retain the key
     * as well so it doesn't change out from under us */
    keys[index] = *key;
    values[index] = *value;
    return index;
  } else {
    PANIC("Duplicate key found while rehashing hash table somehow!");
  }
}

static usize set_key(hashtbl *t, hVal *key, hVal *value) {
  usize index = find_index_by_key(t, key);

  hVal *keys;
  sbVar *values;
  get_ptrs_for_tbl(t, &keys, &values);

  /* now, we either found the current entry for this key,
   * or we found an empty slot that fits this key. */
  if (keys[index].type == IT_NOTHING) {
    /* key was not here before, so we need to retain the key
     * as well so it doesn't change out from under us */
    sbV_retain(key);
    keys[index] = *key;
    t->used ++;
    t->n_elems ++;
    if (t->used >= t->capacity * 3 / 4) {
      set_hashtbl_size(t, t->capacity * 3 / 2, TRUE);
    }
  } else {
    /* replacing something that already exists. we can keep
     * the key the same, but need to release the previous value. */
    hVal to_replace = sbVar_get_value(&values[index]);
    sbV_release(&to_replace);
  }

  sbV_retain(value);
  values[index] = (sbVar) { .value = *value };

  return index;
}

static void delete_key(hashtbl *t, hVal *key) {
  usize index = find_index_by_key(t, key);
  hVal *keys;
  sbVar *values;
  get_ptrs_for_tbl(t, &keys, &values);
  hVal to_delete = sbVar_get_value(&values[index]);
  sbV_release(&to_delete);
  keys[index].type = ITX_TOMBSTONE;
  t->n_elems --;
}

static hVal delete_key_and_return(hashtbl *t, hVal *key) {
  usize index = find_index_by_key(t, key);
  hVal *keys;
  sbVar *values;
  get_ptrs_for_tbl(t, &keys, &values);
  hVal to_return = sbVar_get_value(&values[index]);
  keys[index].type = ITX_TOMBSTONE;
  t->n_elems --;
  return to_return;
}

static hashtbl *find_tbl_for_handle(hHash handle) {
  return sbPool_get_entry(&g_hashtable_pool, handle);
}

static hashtbl *new_tbl(usize initial_size) {
  usize index;
  hashtbl *t = sbPool_alloc(&g_hashtable_pool, &index);
  set_hashtbl_size(t, initial_size, FALSE);
  t->handle = index;
  return t;
}
