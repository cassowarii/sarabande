#include "lib/libtable.h"

#include "data/hashtable.h"
#include "vm/exec.h"
#include "mem/debug.h"

/*
typedef sbLibTable {
  usize used;
  usize capacity;
  hSymbol *keys;
  sbLibFunc *funcs;
} sbLibTable;
*/

extern void sbList_create_methods();
extern void sbString_create_methods();
extern void sbInteger_create_methods();

static void insert_func(hSymbol *keys, sbLibFunc *funcs, usize capacity, hSymbol key, sbLibFunc func);

void sbLibTable_initialize(hLibTable t, usize capacity) {
  *t = (sbLibTable) {0};
  if (capacity < 16) {
    capacity = 16;
  }
  t->keys = calloc(capacity, sizeof(hSymbol));
  t->funcs = calloc(capacity, sizeof(sbLibFunc));
  t->capacity = capacity;
}

void sbLibTable_deinitialize(hLibTable t) {
  free(t->keys);
  free(t->funcs);
  *t = (sbLibTable) {0};
}

void sbLib_sys_init() {
  sbLibTable_initialize(&g_list_methods, 16);
  sbLibTable_initialize(&g_string_methods, 16);
  sbLibTable_initialize(&g_integer_methods, 16);

  sbList_create_methods();
  sbString_create_methods();
  sbInteger_create_methods();
}

void sbLib_sys_deinit() {
  sbLibTable_deinitialize(&g_list_methods);
  sbLibTable_deinitialize(&g_string_methods);
  sbLibTable_deinitialize(&g_integer_methods);
}

void sbLib_resolve_method(hVm vm) {
  hV *target = sbVm_pop(vm);
  hV *argc = sbVm_pop(vm);
  if (argc->type != IT_INTEGER) {
    CHECK("argc of send should be integer!");
  }
  /* subtract 1 because the method name is itself a param */
  usize num_params = argc->integer - 1;
  hV *method_name_val = sbVm_peek(vm, num_params);
  if (method_name_val->type != IT_SYMBOL) {
    /* TODO this may become not true */
    PANIC("method name must be symbol!");
  }

  hLibTable table_to_use = NULL;
  switch(target->type) {
    case IT_LIST:
      table_to_use = &g_list_methods;
      break;
    case IT_STRING:
      table_to_use = &g_string_methods;
      break;
    case IT_INTEGER:
      table_to_use = &g_integer_methods;
      break;
    default:
      PANIC("Have not implemented this method table yet!");
  }

  sbLibFunc f = sbLibTable_find(table_to_use, method_name_val->symbol);
  if (f) {
    f(vm, target, num_params);
  } else {
    PANIC("invalid method name for type %lld: %s", (long long)target->type, sbSymbol_name(method_name_val->symbol));
  }
}

sbLibFunc sbLibTable_find(hLibTable t, hSymbol key) {
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

void sbLibTable_register(hLibTable t, const char *method_name, usize method_name_length, sbLibFunc behavior) {
  if (t->used > t->capacity / 4 * 3) {
    /* grow table and rehash */
    usize new_capacity = t->capacity * 2;
    hSymbol *new_keys = calloc(new_capacity, sizeof(hSymbol));
    sbLibFunc *new_funcs = calloc(new_capacity, sizeof(sbLibFunc));

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

static void insert_func(hSymbol *keys, sbLibFunc *funcs, usize capacity, hSymbol key, sbLibFunc func) {
  sbHashValue hash = sbHash_hash_bytes((const char*)&key, sizeof(hSymbol));
  usize index = hash % capacity;

  while (keys[index] != 0) {
    index ++;
    index %= capacity;
  }

  keys[index] = key;
  funcs[index] = func;
}
