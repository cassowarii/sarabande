#ifndef __SARABANDE_HANDLE_H__
#define __SARABANDE_HANDLE_H__

#include "global.h"

#define IT_FLAG_BOUND_METHOD (1ULL << 62)

#define HVINT(n) ((hV) { .type = IT_INTEGER, .integer = n })
#define HVFLOAT(f) ((hV) { .type = IT_FLOAT, .float_val = f })
#define HVSTR(s) ((hV) { .type = IT_STRING, .string = s })
#define HVSYM(s) ((hV) { .type = IT_SYMBOL, .symbol = s })
#define HVBOOL(b) ((hV) { .type = IT_BOOLEAN, .boolean = b })
#define HVLIST(l) ((hV) { .type = IT_LIST, .list = l })
#define HVREF(r) ((hV) { .type = IT_REF, .ref = r })
#define HVNIL ((hV) { .type = IT_NIL })
#define HVNOTHING ((hV) {0})
#define HVFUNC(i, c) ((hV) { .type = i, .closure = c })
#define HVBUILTIN(b) ((hV) { .type = IT_BUILTIN, .builtin = b })
#define HVBOUNDMETHOD(m, t) ((hV) { .type = m | IT_FLAG_BOUND_METHOD, .ref = t })
#define HVMODULE(m) ((hV) { .type = IT_MODULE, .module = m })

struct sbVm;
struct sbLibTable;

typedef u64 hHash;
typedef u64 hString;
typedef u64 hSymbol;
typedef i64 hInteger;
typedef u64 hList;
typedef u64 hRef;
typedef u64 hClosure;
typedef void (*sbBuiltinFunc)(struct sbVm*, usize);

enum intrinsic_type {
  IT_NOTHING,          // sentinel for "no value here"
  IT_NIL = -1,         // nil ("there is a value here, but it's nil")
  IT_BOOLEAN = -2,     // true / false
  IT_STRING = -3,      // `abcdefg`
  IT_SYMBOL = -4,      // :hello
  IT_INTEGER = -5,     // 324892
  IT_FLOAT = -6,       // 0.0345
  IT_DATETIME = -7,    // unix timestamp
  IT_REF = -8,         // pointer &abc
  IT_LIST = -9,        // list [1, 3, 5, 7]
  IT_HASH = -10,       // hash {a: 1, b: 2}
  IT_BUILTIN = -11,    // c function
  IT_MODULE = -12,     // builtin module (math, op...)
  ITX_TOMBSTONE = -13, // <hashtable_tombstone>
};

typedef struct hV {
  i64 type;
  union {
    hString string;
    hSymbol symbol;
    hHash hash;
    hList list;
    hInteger integer;
    hClosure closure;
    hRef ref;
    u64 boolean;
    double float_val;
    sbBuiltinFunc builtin;
    struct sbLibTable *module;
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
hV sbV_empty_list(usize capacity);
hV sbV_empty_hash(usize capacity);

flag sbV_c_eq(const hV *a, const hV *b);
flag sbV_c_falsy(const hV *a);

void sbV_retain(const hV *a);
void sbV_release(const hV *a);

#endif
