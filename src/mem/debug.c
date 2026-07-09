#include "mem/debug.h"

void print_memory_bytes(const char *where, usize how_many) {
  for (usize i = 0; i < how_many; i++) {
    printf("%02X ", ((u8*)where)[i]);
  }
  printf("\n");
}
