#include "symbol.h"

#include "data/string.h"
#include "data/hashtable.h"

#define NAME_BLOCK_SIZE 4096

/* TODO globals. though, this might be ok? */
hHash symbol_table;
u64 next_symbol_id = 0;
sbArena symbol_text_arena;

hSymbol new_symbol(const char *name, usize length);

hSymbol sbSymbol_from_bytes(const char *text, usize length) {
  hV symkey = {
    .type = IT_STRING,
    .string = sbString_new(text, length),
  };

  hV result = sbHash_find(symbol_table, &symkey);

  if (result.type == IT_NOTHING) {
    /* add new symbol */
    hV symval = {
      .type = IT_SYMBOL,
      .symbol = new_symbol(text, length),
    };

    sbHash_insert(symbol_table, &symkey, &symval);
    return symval.symbol;
  } else if (result.type == IT_SYMBOL) {
    /* return already existing symbol */
    return result.symbol;
  } else {
    PANIC("Only symbol values are allowed in the symbol table! (%lld)", (long long)result.type);
  }
}

hSymbol sbSymbol_from_string(hString str) {
  char scratch[8];
  usize length;
  const char *data = sbString_get_value(str, scratch, &length);
  return sbSymbol_from_bytes(data, length);
}

void sbSymbol_sys_init() {
  symbol_table = sbHash_create(256);
  sbArena_initialize(&symbol_text_arena, NAME_BLOCK_SIZE);
  /* create symbol 0 as empty symbol */
  sbSymbol_from_bytes("", 0);
}

void sbSymbol_sys_deinit() {
  // nothing
}

char *sbSymbol_name(hSymbol sym) {
  return (char*)sym;
}

/* --- */

hSymbol new_symbol(const char *name, usize length) {
  char *name_loc = sbArena_alloc(&symbol_text_arena, length + 1);
  memcpy(name_loc, name, length);
  name_loc[length] = '\0';
  return (hSymbol)name_loc;
}

