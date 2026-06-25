#include "common.h"

#include "data/handle.h"

/* Open-hashing hash table of object handle keys -> object handle values. */

typedef u64 sbHashValue;

void sbHash_sys_init();

void sbHash_sys_deinit();

sbHashValue sbHash_hash_bytes(const char *bytes, usize length);

sbHashValue sbHash_hash_obj(hV *obj);

hHash sbHash_create(usize initial_size);

hV sbHash_find(hHash h, hV *key);

void sbHash_insert(hHash h, hV *key, hV *value);

hV sbHash_find_or_insert(hHash h, hV *key, hV *value);

void sbHash_delete(hHash h, hV *key, hV *value);
