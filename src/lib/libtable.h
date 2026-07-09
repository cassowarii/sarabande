#include "common.h"

#include "data/symbol.h"

#define LIB_REGISTER(t, n, b) (sbLibTable_register(t, n, sizeof(n) - 1, b))

typedef void (*sbLibFunc)(hVm, hV*, usize);

typedef struct sbLibTable {
  usize used;
  usize capacity;
  hSymbol *keys;
  sbLibFunc *funcs;
} sbLibTable;

extern sbLibTable g_list_methods;
extern sbLibTable g_string_methods;
extern sbLibTable g_integer_methods;

typedef sbLibTable *hLibTable;

void sbLibTable_initialize(hLibTable t, usize capacity);

void sbLibTable_deinitialize(hLibTable t);

sbLibFunc sbLibTable_find(hLibTable t, hSymbol key);

void sbLibTable_register(hLibTable t, const char *method_name, usize method_name_length, sbLibFunc behavior);

void sbLib_resolve_method(hVm vm);

void sbLib_sys_init();

void sbLib_sys_deinit();

void sbList_method(hVm vm);
