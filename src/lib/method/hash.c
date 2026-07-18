#include "common.h"

#include "lib/table.h"
#include "lib/sentinel.h"
#include "data/integer.h"
#include "data/hashtable.h"
#include "vm/exec.h"

sbLibTable g_hash_methods;

static void to_hash(hVm vm, hVal *target, usize num_params) {
  /* to_hash for a hash just returns itself */
  sbVm_push_immediate(vm, target);
}

static void length(hVm vm, hVal *target, usize num_params) {
  usize length = sbHash_get_size(target->hash);
  sbVm_push_immediate(vm, &HVINT(length));
}

static void get_index(hVm vm, hVal *target, usize num_params) {
  hVal *key = sbVm_pop(vm);
  hVal *result = sbHash_find(target->hash, key);
  if (result == IT_NOTHING) {
    sbVm_push_immediate(vm, &HVNIL);
  } else {
    sbVm_push(vm, result);
  }
}

void sbHash_create_methods(void) {
  sbLibTable_initialize(&g_hash_methods, 16, TRUE);
  REGISTER_METHOD(&g_hash_methods, "length", &PROPERTY(length));
  REGISTER_METHOD_SYM(&g_hash_methods, S_OP_INDEX, &METHOD(get_index, 1, 1));
  REGISTER_METHOD_SYM(&g_hash_methods, S_OP_TO_HASH, &PROPERTY(to_hash));
}
