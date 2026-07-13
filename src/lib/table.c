#include "lib/table.h"

#include "data/hashtable.h"
#include "data/symbol.h"
#include "vm/exec.h"
#include "mem/debug.h"

/*
typedef sbLibTable {
  usize used;
  usize capacity;
  hSymbol *keys;
  union {
    sbLibMethod *methods;
    hV *values;
  };
} sbLibTable;
*/

static void insert_method(hSymbol *keys, sbLibMethod *methods, usize capacity, hSymbol key, sbLibMethod *method);
static void insert_value(hSymbol *keys, hV *values, usize capacity, hSymbol key, hV *value);
static void check_grow_table(hLibTable t);

void sbLibTable_initialize(hLibTable t, usize capacity, flag method_table) {
  *t = (sbLibTable) {0};
  if (capacity < 16) {
    capacity = 16;
  }
  t->keys = calloc(capacity, sizeof(hSymbol));
  if (method_table) {
    t->method_table = TRUE;
    t->methods = calloc(capacity, sizeof(sbLibMethod));
  } else {
    t->values = calloc(capacity, sizeof(hV));
  }
  t->capacity = capacity;
}

void sbLibTable_deinitialize(hLibTable t) {
  free(t->keys);
  if (t->method_table) {
    free(t->methods);
  } else {
    free(t->values);
  }
  *t = (sbLibTable) {0};
}

sbLibMethod *sbLibTable_find_method(hLibTable t, hSymbol key) {
  if (!t->method_table) CHECK("cannot find method in non-method table");

  sbHashValue hash = sbHash_hash_bytes((const char*)&key, sizeof(hSymbol));
  usize index = hash % t->capacity;
  while (t->keys[index] && t->keys[index] != key) {
    index ++;
    index %= t->capacity;
  }

  if (t->keys[index]) {
    return &t->methods[index];
  } else {
    return NULL;
  }
}

hV *sbLibTable_find_value(hLibTable t, hSymbol key) {
  if (t->method_table) CHECK("cannot find value in method table");

  sbHashValue hash = sbHash_hash_bytes((const char*)&key, sizeof(hSymbol));
  usize index = hash % t->capacity;
  while (t->keys[index] && t->keys[index] != key) {
    index ++;
    index %= t->capacity;
  }

  if (t->keys[index]) {
    return &t->values[index];
  } else {
    return NULL;
  }
}

void sbLibTable_register_method_sym(hLibTable t, hSymbol key, sbLibMethod *method) {
  if (!t->method_table) CHECK("cannot put method in non-method table");

  check_grow_table(t);

  insert_method(t->keys, t->methods, t->capacity, key, method);
  t->used ++;
}

void sbLibTable_register_method(hLibTable t, const char *method_name, usize method_name_length, sbLibMethod *method) {
  hSymbol sym = sbSymbol_from_bytes(method_name, method_name_length);
  sbLibTable_register_method_sym(t, sym, method);
}

void sbLibTable_register_value_sym(hLibTable t, hSymbol key, hV *value) {
  if (t->method_table) CHECK("cannot put value in method table");

  check_grow_table(t);

  insert_value(t->keys, t->values, t->capacity, key, value);
  t->used ++;
}

void sbLibTable_register_value(hLibTable t, const char *value_name, usize value_name_length, hV *value) {
  hSymbol sym = sbSymbol_from_bytes(value_name, value_name_length);
  sbLibTable_register_value_sym(t, sym, value);
}

/* --- */

static void insert_method(hSymbol *keys, sbLibMethod *methods, usize capacity, hSymbol key, sbLibMethod *method) {
  sbHashValue hash = sbHash_hash_bytes((const char*)&key, sizeof(hSymbol));
  usize index = hash % capacity;

  while (keys[index] != 0) {
    index ++;
    index %= capacity;
  }

  keys[index] = key;
  methods[index] = *method;
}

static void insert_value(hSymbol *keys, hV *values, usize capacity, hSymbol key, hV *value) {
  sbHashValue hash = sbHash_hash_bytes((const char*)&key, sizeof(hSymbol));
  usize index = hash % capacity;

  while (keys[index] != 0) {
    index ++;
    index %= capacity;
  }

  keys[index] = key;
  values[index] = *value;
}

static void check_grow_table(hLibTable t) {
  if (t->used > t->capacity / 4 * 3) {
    /* grow table and rehash */
    usize new_capacity = t->capacity * 2;
    hSymbol *new_keys = calloc(new_capacity, sizeof(hSymbol));

    if (t->method_table) {
      sbLibMethod *new_methods = calloc(new_capacity, sizeof(sbLibMethod));

      for (usize i = 0; i < t->capacity; i++) {
        if (t->keys[i] != 0) {
          insert_method(new_keys, new_methods, new_capacity, t->keys[i], &t->methods[i]);
        }
      }

      free(t->methods);
      t->methods = new_methods;
    } else {
      hV *new_values = calloc(new_capacity, sizeof(hV));

      for (usize i = 0; i < t->capacity; i++) {
        if (t->keys[i] != 0) {
          insert_value(new_keys, new_values, new_capacity, t->keys[i], &t->values[i]);
        }
      }

      free(t->values);
      t->values = new_values;
    }

    free(t->keys);
    t->capacity = new_capacity;
    t->keys = new_keys;
  }
}
