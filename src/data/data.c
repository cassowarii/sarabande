#include "data/data.h"

#include "data/string.h"
#include "data/hashtable.h"
#include "data/symbol.h"
#include "data/integer.h"
#include "data/list.h"
#include "data/reference.h"
#include "data/closure.h"

#include "lib/lib.h"

extern sbPool g_closure_pool;

void data_sys_init() {
  sbHash_sys_init();
  sbSymbol_sys_init();
  sbString_sys_init();
  sbRef_sys_init();
  sbInteger_sys_init();
  sbClosure_sys_init();
  sbList_sys_init();

  sbLib_sys_init();
}

void data_sys_deinit() {
  sbLib_sys_deinit();

  sbList_sys_deinit();
  sbClosure_sys_deinit();
  sbInteger_sys_deinit();
  sbRef_sys_deinit();
  sbString_sys_deinit();
  sbSymbol_sys_deinit();
  sbHash_sys_deinit();
}
