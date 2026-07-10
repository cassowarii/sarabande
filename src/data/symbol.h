#include "common.h"

#include "data/value.h"

#define SENTINEL(str) (sbSymbol_sentinel(str, sizeof(str)-1))

void sbSymbol_sys_init();
void sbSymbol_sys_deinit();

hSymbol sbSymbol_from_string(hString str);

hSymbol sbSymbol_from_bytes(const char *text, usize length);

hSymbol sbSymbol_sentinel(const char *text, usize length);

char *sbSymbol_name(hSymbol sym);
