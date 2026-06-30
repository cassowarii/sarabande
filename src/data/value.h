#ifndef __SARABANDE_HANDLE_H__
#define __SARABANDE_HANDLE_H__

#include "common.h"

#define HVINT(n) ((hV) { .type = IT_INTEGER, .integer = n })
#define HVSTR(s) ((hV) { .type = IT_STRING, .string = OBJSL(s) })
#define HVBOOL(b) ((hV) { .type = IT_BOOLEAN, .boolean = b })
#define HVFUNC(i) ((hV) { .type = IT_FUNCTION, .data = i })

typedef u64 hHash;
typedef u64 hString;
typedef u64 hSymbol;
typedef i64 hInteger;

enum intrinsic_type {
  IT_NOTHING,     // sentinel for "no value here"
  IT_NIL,         // nil ("there is a value here, but it's nil")
  IT_BOOLEAN,     // true / false
  IT_STRING,      // `abcdefg`
  IT_SYMBOL,      // :hello
  IT_INTEGER,     // 324892
  IT_FLOAT,       // 0.0345
  IT_DATETIME,    // unix timestamp
  IT_REF,         // pointer \abc
  IT_LIST,        // list [1, 3, 5, 7]
  IT_HASH,        // hash {a: 1, b: 2}
  IT_FUNCTION,    // function => a, b { a + b }
  ITX_TOMBSTONE,  // <hashtable_tombstone>
};

typedef struct hV {
  u64 type;
  union {
    hString string;
    hSymbol symbol;
    hHash hash;
    hInteger integer;
    u64 boolean;
    double float_val;
    u64 data;
  };
} hV;

hV sbV_nil();
hV sbV_string(hString str);
hV sbV_symbol(hSymbol sym);
hV sbV_float(double fl);
hV sbV_hash(hHash hash);
hV sbV_int(hInteger i);
hV sbV_boolean(flag b);
hV sbV_function(u64 id);

flag sbV_c_eq(const hV *a, const hV *b);
flag sbV_c_falsy(const hV *a);

void sbV_retain(const hV *a);
void sbV_release(const hV *a);

#endif
