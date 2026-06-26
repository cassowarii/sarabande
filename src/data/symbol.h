#include "common.h"

#include "data/value.h"

void sbSymbol_sys_init();

hSymbol sbSymbol_from_string(hString str);

hSymbol sbSymbol_from_bytes(const char *text, usize length);

char *sbSymbol_name(hSymbol sym);
