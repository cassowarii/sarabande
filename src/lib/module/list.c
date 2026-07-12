#include "common.h"

#include "lib/table.h"
#include "lib/sentinel.h"
#include "data/list.h"
#include "data/string.h"
#include "data/symbol.h"
#include "data/integer.h"
#include "vm/exec.h"

sbLibTable g_list_module;

static void do_iota(hVm vm, usize argc, i32 offset) {
  if (argc != 1 && argc != 2) {
    if (offset == 1) {
      PANIC("list::iota1 takes 1 or 2 arguments");
    } else {
      PANIC("list::iota takes 1 or 2 arguments");
    }
  }

  hV *first = sbVm_pop(vm);

  hInteger min, max;
  if (argc == 1) {
    if (first->type != IT_INTEGER) {
      if (offset == 1) {
        PANIC("list::iota1's argument must be an integer!");
      } else {
        PANIC("list::iota's argument must be an integer!");
      }
    }
    min = offset;
    max = first->integer + offset;
  } else {
    hV *second = sbVm_pop(vm);
    if (first->type != IT_INTEGER || second->type != IT_INTEGER) {
      if (offset == 1) {
        PANIC("list::iota1's arguments must be integers!");
      } else {
        PANIC("list::iota's arguments must be integers!");
      }
    }
    min = first->integer + offset;
    max = second->integer + offset;
  }

  hList result = sbList_new((usize)sbInteger_diff(max, min));
  hInteger count = min;
  while (sbInteger_lt(count, max)) {
    sbList_append(result, &HVINT(count));
    count = sbInteger_sum(count, 1);
  }

  sbVm_push_immediate(vm, &HVLIST(result));
}

static void iota(hVm vm, usize argc) {
  do_iota(vm, argc, 0);
}

static void iota1(hVm vm, usize argc) {
  do_iota(vm, argc, 1);
}

void sbLib_loadmodule_list() {
  sbLibTable_initialize(&g_list_module, 16, FALSE);
  REGISTER_VALUE(&g_list_module, "iota", &HVBUILTIN(iota));
  REGISTER_VALUE(&g_list_module, "iota1", &HVBUILTIN(iota1));
  REGISTER_VALUE(&g_list_module, "convert", &HVSYM(S_OP_TO_LIST));
}
