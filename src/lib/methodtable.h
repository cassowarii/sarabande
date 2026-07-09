#include "common.h"

#include "data/symbol.h"

#define LIB_REGISTER(t, n, b) (sbMethodTable_register(t, n, sizeof(n) - 1, b))

typedef struct sbMethodTable {
  usize used;
  usize capacity;
  hSymbol *keys;
  sbLibMethod *funcs;
} sbMethodTable;

extern sbMethodTable g_list_methods;
extern sbMethodTable g_string_methods;
extern sbMethodTable g_integer_methods;

typedef sbMethodTable *hMethodTable;

void sbMethodTable_initialize(hMethodTable t, usize capacity);

void sbMethodTable_deinitialize(hMethodTable t);

sbLibMethod sbMethodTable_find(hMethodTable t, hSymbol key);

void sbMethodTable_register(hMethodTable t, const char *method_name, usize method_name_length, sbLibMethod behavior);
