#include "common.h"

typedef const u64 hString;

hString sbString_alloc(const char *value, usize length);

const char *sbString_get_ptr(hString string, usize *length_out);

void sbString_sys_init();

void sbString_sys_deinit();

