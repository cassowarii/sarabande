#include "data/handle.h"

#include "data/string.h"
#include "data/hashtable.h"

#define FLAG_NONINTRINSIC (1ULL << 63)

hV sbV_nil() {
  return (hV) {0};
}

hV sbV_string(hString str) {
  return (hV) {
    .type = IT_STRING,
    .string = str,
  };
}

flag sbV_eq(hV *a, hV *b) {
  if (a->type != b->type) return FALSE;
  if (a->type == ITX_TOMBSTONE || b->type == ITX_TOMBSTONE) return FALSE;
  if (a->type == IT_NOTHING || b->type == IT_NOTHING) return FALSE;

  if (a->type == IT_STRING) {
    return sbString_eq(a->string, b->string);
  } else {
    return a == b;
  }
}

void sbV_retain(hV *a) {
  /* for some classes (e.g. nil, float), we don't need to retain them at all
   * because they are trivially copiable. some classes (integer, string) need
   * to sometimes be retained but sometimes not, so we delegate to them to
   * figure out if they need to or not. */
  if (a->type == IT_STRING) {
    sbString_clone(a->string);
  }
}

void sbV_release(hV *a) {
  if (a->type == IT_STRING) {
    sbString_release(a->string);
  }
}

/* --- */

flag val_is_intrinsic(hV val) {
  return (val.type & FLAG_NONINTRINSIC) != 0;
}
