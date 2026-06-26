#include "common.h"

#include "data/value.h"

void sbSymbol_sys_init();
void sbSymbol_sys_deinit();

hSymbol sbSymbol_from_string(hString str);

hSymbol sbSymbol_from_bytes(const char *text, usize length);

char *sbSymbol_name(hSymbol sym);
