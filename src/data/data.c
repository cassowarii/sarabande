#include "data/data.h"

#include "data/string.h"
#include "data/hashtable.h"
#include "data/symbol.h"
#include "data/list.h"

void data_sys_init() {
  sbString_sys_init();
  sbHash_sys_init();
  sbSymbol_sys_init();
  sbList_sys_init();
}

void data_sys_deinit() {
  sbList_sys_deinit();
  sbSymbol_sys_deinit();
  sbHash_sys_deinit();
  sbString_sys_deinit();
}
