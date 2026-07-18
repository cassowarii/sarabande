#include "data/value.h"

#include "data/string.h"
#include "data/list.h"
#include "data/hashtable.h"
#include "data/integer.h"

#define FLAG_NONINTRINSIC (1ULL << 63)

hVal sbV_empty_list(usize capacity) {
  return (hVal) {
    .type = IT_LIST,
    .list = sbList_new(capacity),
  };
}

hVal sbV_empty_hash(usize capacity) {
  return (hVal) {
    .type = IT_HASH,
    .hash = sbHash_create(capacity),
  };
}

hVal sbV_empty_string() {
  return HVSTR((hString)0);
}

flag sbV_c_eq(const hVal *a, const hVal *b) {
  if (a->type != b->type) return FALSE;
  if (a->type == ITX_TOMBSTONE || b->type == ITX_TOMBSTONE) return FALSE;
  if (a->type == IT_NOTHING || b->type == IT_NOTHING) return FALSE;

  switch (a->type) {
    case IT_NIL:
      /* if a and b share a type, and it's nil, they must be equal */
      return TRUE;
    case IT_BOOLEAN:
      /* for booleans, they are equal if both zero or neither zero */
      return (a->boolean == 0 && b->boolean == 0) || (a->boolean != 0 && b->boolean != 0);
    case IT_SYMBOL:
      /* symbols are deduplicated so just compare pointers */
      return a->symbol == b->symbol;
    case IT_FLOAT:
      /* floats are just comparable as 64 bit values by cursed float-logic */
      return a->float_val == b->float_val;
    case IT_STRING:
      /* strings can be compared for equality character by character */
      return sbString_eq(a->string, b->string);
    case IT_INTEGER:
      /* TODO: we need to handle bigints properly. for now, just check int values normally */
      return a->integer == b->integer;
    case IT_HASH:
      /* hashes should (but currently don't) check that they are the same by value.
       * right now, we'll just see if they point to the same hash ("is"). */
      return a->hash == b->hash;
    default:
      PANIC("never implemented == for this case!");
  }
}

flag sbV_c_falsy(const hVal *a) {
  /* only nil and boolean false are false */
  if (a->type == IT_NIL) return TRUE;
  if (a->type == IT_BOOLEAN && !a->boolean) return TRUE;
  return FALSE;
}

void sbV_retain(const hVal *a) {
  /* for some classes (e.g. nil, float), we don't need to retain them at all
   * because they are trivially copiable. some classes (integer, string) need
   * to sometimes be retained but sometimes not, so we delegate to them to
   * figure out if they need to or not. */
  switch (a->type) {
    case IT_STRING:
      sbString_clone(a->string);
      break;
    case IT_INTEGER:
      sbInteger_retain(a->string);
      break;
    default:
      /* nothing */
      break;
  }
}

void sbV_release(const hVal *a) {
  switch (a->type) {
    case IT_STRING:
      sbString_release(a->string);
      break;
    case IT_INTEGER:
      sbInteger_release(a->string);
      break;
    default:
      /* nothing */
      break;
  }
}

/* --- */

flag val_is_intrinsic(hVal val) {
  return (val.type & FLAG_NONINTRINSIC) != 0;
}
