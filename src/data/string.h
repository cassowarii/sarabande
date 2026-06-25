#include "common.h"

#define OBJSL(value) (sbString_new(value, sizeof(value) - 1))

typedef u64 hString;

/* create hString:
 *
 *    hString hs = sbString_new(charptr, len); // length does not include NUL
 *    hString hs = sbString_new("Hello world!", 12);
 *    hString hs = OBJSL("Hello world!");
 *
 * get hString length:
 *
 *    usize length;
 *    sbString_get_value(hs, NULL, &length);
 *
 * hString -> c-string to read:
 *
 *    char scratch[8]; // must be at least 8
 *    const char *value = sbString_get_value(hs, scratch, NULL);
 *
 * of course, you can also get both the content and length at once by passing both parameters.
 *
 * hString -> c-string to edit:
 *
 *    char scratch[8]; // must be at least 8
 *    usize length_needed = ...;
 *    char *buffer_to_modify = sbString_get_mutable_buffer(hs, length_needed, scratch, NULL);
 *    // do something with buffer_to_modify
 *    sbString_fix_new_value(&hs, buffer_to_modify, length_needed);
 *
 * if you don't intend to change the string length, you can do it like:
 *
 *    char scratch[8]; // must be at least 8
 *    usize string_length;
 *    char *buffer_to_modify = sbString_get_mutable_buffer(hs, 0, scratch, &string_length);
 *    // do something with buffer_to_modify; string length is in string_length
 *    sbString_fix_new_value(&hs, buffer_to_modify, string_length);
 *
 * note that the (const) char * returned may refer to the 'scratch' buffer, so these
 * pointers are not generally safe to return or save. just use hString for long-term string
 * objects, only use c-strings when temporarily manipulating string data
 *
 * to make a copy of hString:
 *
 *    hString copied_hs = sbString_clone(hs);
 *
 * to release a copy of hString:
 *
 *    sbString_release(copied_hs);
 *
 * these copies are not actually copies, but rather are used to track a reference count to
 * lazily copy strings when modification makes them necessary. the reference counter is not
 * actually used for memory management, so failing to release an hString will not result in
 * a memory leak, but the refcount is used to reduce the number of copies, so it pays to
 * release clones that you're done with. furthermore, since objects aren't actually freed by
 * their reference count hitting 0, you can reduce the refcount *before* doing something like
 * a concatenation to reuse a string object that has 0 refs.
 */

hString sbString_new(const char *value, usize length);

const char *sbString_get_value(hString handle, char *buffer_i_might_use, usize *length_out);

char *sbString_get_mutable_buffer(hString *handle, usize length_req, char *buffer_i_might_use, usize *length_out);
void sbString_fix_new_value(hString *handle, const char *new_value, usize length);

hString sbString_clone(hString handle);
void sbString_release(hString handle);

hString sbString_joined(hString a, hString b);

void sbString_sys_init();
void sbString_sys_deinit();
