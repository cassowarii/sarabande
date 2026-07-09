#include "lib/methodtable.h"

#include "data/hashtable.h"
#include "vm/exec.h"
#include "mem/debug.h"

/*
typedef sbMethodTable {
  usize used;
  usize capacity;
  hSymbol *keys;
  sbLibMethod *funcs;
} sbMethodTable;
*/

static void insert_func(hSymbol *keys, sbLibMethod *funcs, usize capacity, hSymbol key, sbLibMethod func);

void sbMethodTable_initialize(hMethodTable t, usize capacity) {
  *t = (sbMethodTable) {0};
  if (capacity < 16) {
    capacity = 16;
  }
  t->keys = calloc(capacity, sizeof(hSymbol));
  t->funcs = calloc(capacity, sizeof(sbLibMethod));
  t->capacity = capacity;
}

void sbMethodTable_deinitialize(hMethodTable t) {
  free(t->keys);
  free(t->funcs);
  *t = (sbMethodTable) {0};
}

sbLibMethod sbMethodTable_find(hMethodTable t, hSymbol key) {
  sbHashValue hash = sbHash_hash_bytes((const char*)&key, sizeof(hSymbol));
  usize index = hash % t->capacity;
  while (t->keys[index] && t->keys[index] != key) {
    index ++;
    index %= t->capacity;
  }

  if (t->keys[index]) {
    return t->funcs[index];
  } else {
    return NULL;
  }
}

void sbMethodTable_register(hMethodTable t, const char *method_name, usize method_name_length, sbLibMethod behavior) {
  if (t->used > t->capacity / 4 * 3) {
    /* grow table and rehash */
    usize new_capacity = t->capacity * 2;
    hSymbol *new_keys = calloc(new_capacity, sizeof(hSymbol));
    sbLibMethod *new_funcs = calloc(new_capacity, sizeof(sbLibMethod));

    for (usize i = 0; i < t->capacity; i++) {
      if (t->keys[i] != 0) {
        insert_func(new_keys, new_funcs, new_capacity, t->keys[i], t->funcs[i]);
      }
    }

    free(t->keys);
    free(t->funcs);
    t->keys = new_keys;
    t->funcs = new_funcs;
    t->capacity = new_capacity;
  }

  hSymbol sym = sbSymbol_from_bytes(method_name, method_name_length);
  insert_func(t->keys, t->funcs, t->capacity, sym, behavior);
  t->used ++;
}

/* --- */

static void insert_func(hSymbol *keys, sbLibMethod *funcs, usize capacity, hSymbol key, sbLibMethod func) {
  sbHashValue hash = sbHash_hash_bytes((const char*)&key, sizeof(hSymbol));
  usize index = hash % capacity;

  while (keys[index] != 0) {
    index ++;
    index %= capacity;
  }

  keys[index] = key;
  funcs[index] = func;
}
