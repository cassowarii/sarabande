#include "common.h"

#define REGISTER_METHOD(t, n, m) (sbLibTable_register_method(t, n, sizeof(n) - 1, m))
#define REGISTER_METHOD_SYM(t, s, m) (sbLibTable_register_method_sym(t, s, m))

#define REGISTER_VALUE(t, n, v) (sbLibTable_register_value(t, n, sizeof(n) - 1, v))
#define REGISTER_VALUE_SYM(t, s, v) (sbLibTable_register_value_sym(t, s, v))

typedef struct sbLibTable {
  usize used;
  usize capacity;
  hSymbol *keys;
  flag method_table;
  union {
      sbLibMethod *methods;
      hV *values;
  };
} sbLibTable;

typedef sbLibTable *hLibTable;

void sbLibTable_initialize(hLibTable t, usize capacity, flag method_table);

void sbLibTable_deinitialize(hLibTable t);

sbLibMethod *sbLibTable_find_method(hLibTable t, hSymbol key);

hV *sbLibTable_find_value(hLibTable t, hSymbol key);

void sbLibTable_register_method(hLibTable t, const char *method_name, usize method_name_length, sbLibMethod *method);

void sbLibTable_register_method_sym(hLibTable t, hSymbol key, sbLibMethod *method);

void sbLibTable_register_value(hLibTable t, const char *value_name, usize value_name_length, hV *value);

void sbLibTable_register_value_sym(hLibTable t, hSymbol key, hV *value);
