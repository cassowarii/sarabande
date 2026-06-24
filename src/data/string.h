#include "common.h"

typedef u64 hString;

hString sbString_new(const char *value, usize length);

/* buffer_i_might_use should be a pointer to a 8-character (at least) stack allocated buffer.
 * if tinystr, it will use this to store the string data returned, and then return a pointer
 * to the buffer. if not tinystr, it will not use the buffer and will return a pointer to
 * somewhere else. */
const char *sbString_get_value(hString handle, char *buffer_i_might_use, usize *length_out);

void sbString_sys_init();

void sbString_sys_deinit();

