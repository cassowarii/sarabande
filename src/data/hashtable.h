#include "common.h"

#include "data/value.h"

/* Open-hashing hash table of object handle keys -> object handle values. */

typedef u64 sbHashValue;

void sbHash_sys_init();

void sbHash_sys_deinit();

sbHashValue sbHash_hash_bytes(const char *bytes, usize length);

sbHashValue sbHash_hash_obj(hVal *obj);

hHash sbHash_create(usize initial_size);

hVal *sbHash_find(hHash h, hVal *key);

void sbHash_insert(hHash h, hVal *key, hVal *value);

hVal *sbHash_find_or_insert(hHash h, hVal *key, hVal *value);

void sbHash_delete(hHash h, hVal *key, hVal *value);

usize sbHash_get_size(hHash h);
