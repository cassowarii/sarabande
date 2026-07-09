#include "data/string.h"

#include "gc/gcinfo.h"
#include "gc/rc.h"
#include "vm/exec.h"
#include "data/list.h"
#include "data/symbol.h"

#define INLINE_BUFFER_SIZE 256
#define STRINGS_PER_BLOCK 256
#define METHOD_IS(name) (!sbstrncmp(method_name, name, sizeof(name)))

/* highest bit unset on handle value means it is a tinystr. this is
 * so that 0x0000_0000_0000_0000 represents an empty string */
#define FLAG_NONTINY (1ULL << 63)

/* if string length < REQ_ALLOC_LENGTH, we can just put the
 * string in the handle itself. this should be 64 / 8 = 8,
 * we need to allocate for an 8 char string because tinystr
 * uses one byte to store the string length as well. */
#define REQ_ALLOC_LENGTH (sizeof(hString)/sizeof(char))

/* note: policy here is that, for consistency, all length values *don't*
 * account for the null terminator, so all buffers' allocations should
 * be one byte longer than their length and hold a NUL there.
 * e.g. tinystr can hold max of 7 characters + a length, but the scratch
 * buffers taken by certain functions should be 8 bytes, because we need
 * to put a NUL at the end */

/* TODO: Move these globals into some kind of "execution context" struct
 * that we can pass around to be reentrant */
sbPool g_string_pool;

typedef struct strentry {
  GCINFO gc;
  usize length;
  hString handle;
  flag is_external;
  flag allocated;
  union {
    char inline_value[INLINE_BUFFER_SIZE];
    sbBuffer somewhere_else_value;
  };
} strentry;

static const char *get_ptr_of_entry(strentry *e);
static char *get_mutable_ptr_of_entry(strentry *e, usize length, hString *handle);
static strentry *find_entry_for_handle(hString handle);
static strentry *new_entry(usize length);
static void place_str_at_offset(strentry *e, usize offset, const char *str, usize length);
static int buffers_eq(const char *a_ptr, usize a_length, const char *b_ptr, usize b_length);
static int buffers_cmp(const char *a_ptr, usize a_length, const char *b_ptr, usize b_length);
static flag is_tinystr(hString handle);
static usize tinystr_length(hString handle);
static void tinystr_into_buffer(char *buffer, hString handle, usize max_length);
static hString tinystr_from_buffer(const char *buffer, usize length);
static void set_entry_size(strentry *e, usize length);

void sbString_sys_init() {
  sbPool_initialize(&g_string_pool, sizeof(strentry), STRINGS_PER_BLOCK);
}

void sbString_sys_deinit() {
  /* TODO free these i guess, or don't deinitialize this buffer, right now this
   * would just drop the pointers to no benefit. look, i'm going to write a
   * garbage collector, ok? give me a second */
  //sbPool_deinitialize(&g_string_pool);
}

hString sbString_new(const char *value, usize length) {
  if (length >= REQ_ALLOC_LENGTH) {
    strentry *e = new_entry(length);
    place_str_at_offset(e, 0, value, length);
    RC_retain(e);
    return e->handle;
  } else {
    /* tinystr */
    return tinystr_from_buffer(value, length);
  }
}

const char *sbString_get_value(hString handle, char *buffer_i_might_use, usize *length_out) {
  if (is_tinystr(handle)) {
    usize tinylength = tinystr_length(handle);
    if (length_out) *length_out = tinylength;
    if (buffer_i_might_use) {
      tinystr_into_buffer(buffer_i_might_use, handle, tinylength);
      /* pointer aliasing ! fun ! we love ! */
      return buffer_i_might_use;
    }
  } else {
    strentry *e = find_entry_for_handle(handle);
    if (length_out) *length_out = e->length;
    if (buffer_i_might_use) return get_ptr_of_entry(e);
  }
  return NULL;
}

hString sbString_clone(hString handle) {
  if (is_tinystr(handle)) {
    /* tinystr's already have value semantics */
    return handle;
  } else {
    /* copying is lazy: we actually just additionally 'retain'
     * the same entry multiple times. if we try to mutate an
     * entry that's referenced in multiple places, then the
     * system will lazily 'copy' the one that is being modified
     * at that point. "COW semantics" */
    strentry *e = find_entry_for_handle(handle);
    RC_retain(e);
    return e->handle;
  }
}

void sbString_release(hString handle) {
  if (is_tinystr(handle)) {
    /* do nothing! */
  } else {
    /* decrement refcount */
    strentry *e = find_entry_for_handle(handle);
    RC_release(e);
  }
}

int sbString_eq(hString a, hString b) {
  char scratch[8];
  usize a_length, b_length;
  const char *a_buf, *b_buf;
  if (is_tinystr(a) && is_tinystr(b)) {
    return a == b;
  } else {
    /* we can use just one scratch because we know they aren't
     * both tinystr */
    a_buf = sbString_get_value(a, scratch, &a_length);
    b_buf = sbString_get_value(b, scratch, &b_length);
    return buffers_eq(a_buf, a_length, b_buf, b_length);
  }
}

int sbString_cmp(hString a, hString b) {
  char a_scratch[8], b_scratch[8];
  usize a_length, b_length;
  const char *a_buf, *b_buf;
  a_buf = sbString_get_value(a, a_scratch, &a_length);
  b_buf = sbString_get_value(b, b_scratch, &b_length);
  return buffers_cmp(a_buf, a_length, b_buf, b_length);
}

hString sbString_concat(hString a, hString b) {
  usize a_length, b_length, final_length;
  char a_scratch[8], b_scratch[8];
  const char *a_str, *b_str;

  a_str = sbString_get_value(a, a_scratch, &a_length);
  b_str = sbString_get_value(b, b_scratch, &b_length);
  final_length = a_length + b_length;

  if (final_length < REQ_ALLOC_LENGTH) {
    /* tinystr */
    char scratch[REQ_ALLOC_LENGTH * 2 - 1];
    memcpy(scratch, a_str, a_length);
    memcpy(scratch + a_length, b_str, b_length);
    return tinystr_from_buffer(scratch, final_length);
  } else {
    strentry *result;
    if (!is_tinystr(a) && find_entry_for_handle(a)->gc.refcount == 0) {
      result = find_entry_for_handle(a);
      set_entry_size(result, final_length);
    } else {
      result = new_entry(final_length);
      place_str_at_offset(result, 0, a_str, a_length);
    }

    place_str_at_offset(result, a_length, b_str, b_length);
    RC_retain(result);

    return result->handle;
  }
}

char *sbString_get_mutable_buffer(hString *handle, usize length_req, char *buffer_i_might_use, usize *length_out) {
  if (!buffer_i_might_use) return NULL;

  flag use_orig_length = (length_req == 0 && length_out != NULL);

  if (is_tinystr(*handle)) {
    usize length = use_orig_length ? tinystr_length(*handle) : length_req;

    if (length < REQ_ALLOC_LENGTH) {
      /* lucky! we can just use the stack buffer! */
      tinystr_into_buffer(buffer_i_might_use, *handle, length);
      if (length_out) *length_out = length;
      return buffer_i_might_use;
    } else {
      /* upgrading a tinystr to a real strentry. need to replace the handle
       * with one that refers to the new allocated version instead */
      strentry *e = new_entry(length);
      char *buffer = get_mutable_ptr_of_entry(e, length, handle);
      tinystr_into_buffer(buffer, *handle, length);
      if (length_out) *length_out = length;
      return buffer;
    }
  } else {
    strentry *e = find_entry_for_handle(*handle);
    usize length = use_orig_length ? e->length : length_req;

    if (length_out) *length_out = length;
    return get_mutable_ptr_of_entry(e, length, handle);
  }
}

/* call after get_mutable_buffer to put the data back in the string if needed
 * (it usually does nothing, but for tinystr it is needed if it remains tiny) */
void sbString_fix_new_value(hString *handle, const char *new_value, usize length) {
  if (length < REQ_ALLOC_LENGTH) {
    /* now tinystr (regardless of what happened before).
     * leave the old string for the garbage collector */
    *handle = tinystr_from_buffer(new_value, length);
  } else {
    strentry *existing_entry = find_entry_for_handle(*handle); /* null if tinystr */
    if (existing_entry && existing_entry->allocated) return; /* we already set it */

    PANIC("invalid handle, or inconsistent string lengths between sbString_get_mutable_buffer"
          " and sbString_fix_new_value!");
  }
}

usize sbString_get_length(hString handle) {
  if (handle & FLAG_NONTINY) {
    /* length is just in the entry */
    return find_entry_for_handle(handle)->length;
  } else {
    /* length is top byte except high bit */
    return (handle >> (8 * 7)) & 0x7F;
  }
}

void sbString_method(hVm vm) {
  hV *target = sbVm_pop(vm);
  if (target->type != IT_STRING) {
    CHECK("can't call sbString_method on something that isn't a string");
  }
  hV *argc = sbVm_pop(vm);
  if (argc->type != IT_INTEGER) {
    CHECK("argc of send should be integer!");
  }
  /* subtract 1 because the method name is itself a param */
  usize num_params = argc->integer - 1;
  hV *method_name_val = sbVm_peek(vm, num_params);
  if (method_name_val->type != IT_SYMBOL) {
    /* TODO this may become not true */
    PANIC("method name for string must be symbol!");
  }

  const char *method_name = sbSymbol_name(method_name_val->symbol);
  /* TODO: Need a better way of resolving these */
  if (METHOD_IS("split")) {
  }
}

/* --- */

static flag is_tinystr(hString handle) {
  return ((handle & FLAG_NONTINY) == 0);
}

static usize tinystr_length(hString handle) {
  return ((handle >> (8 * 7)) & 0x7F);
}

static strentry *find_entry_for_handle(hString handle) {
  if (is_tinystr(handle)) return NULL;
  return sbPool_get_entry(&g_string_pool, handle & ~FLAG_NONTINY);
}

static strentry *duplicate_strentry(strentry *e, usize length) {
  strentry *e_copy = new_entry(length);
  place_str_at_offset(e_copy, 0, get_ptr_of_entry(e), length);
  return e_copy;
}

static int buffers_eq(const char *a_ptr, usize a_length, const char *b_ptr, usize b_length) {
  if (a_length != b_length) return 0;
  for (int i = 0; i < a_length; i++) {
    if (a_ptr[i] != b_ptr[i]) {
      return 0;
    }
  }
  return 1;
}

static int buffers_cmp(const char *a_ptr, usize a_length, const char *b_ptr, usize b_length) {
  usize shorter_length = a_length;
  if (b_length < shorter_length) shorter_length = b_length;
  for (int i = 0; i < shorter_length; i++) {
    if (a_ptr[i] < b_ptr[i]) {
      return 1;
    }
    if (a_ptr[i] > b_ptr[i]) {
      return -1;
    }
  }
  /* equal up to shorter length */
  if (a_length < b_length) {
    return 1;
  } else if (b_length > a_length) {
    return -1;
  }

  return 0;
}

static const char *get_ptr_of_entry(strentry *e) {
  if (e->is_external) {
    return e->somewhere_else_value.data;
  } else {
    return e->inline_value;
  }
}

static char *get_mutable_ptr_of_entry(strentry *e, usize length, hString *handle) {
  /* TODO: When we have static strings, we should copy them to a new
   * entry in order to get a mutable pointer to them. */
  if (e->gc.refcount < 2) {
    /* 0 or 1 refs: can mutate string in place */
    set_entry_size(e, length);

    if (handle) *handle = e->handle;
    if (e->is_external) {
      return e->somewhere_else_value.data;
    } else {
      return e->inline_value;
    }
  } else {
    /* 2+ refs: need to make a copy of the string to mutate so we don't
     * affect the other versions */
    strentry *e_copy = duplicate_strentry(e, length);

    /* transfer one refcount to new string */
    RC_retain(e_copy);
    RC_release(e);

    if (handle) *handle = e_copy->handle;
    if (e->is_external) {
      return e->somewhere_else_value.data;
    } else {
      return e->inline_value;
    }
  }
}

static void tinystr_into_buffer(char *buffer, hString handle, usize max_length) {
  /* we have max_length because we may need to cut off early in some corner cases,
   * like getting a mutable ptr of length 3 for a tinystr of length 6 */
  if (!is_tinystr(handle)) PANIC("%llx is not a tinystr! can't parse!", (long long)handle);

  usize index = 0;
  usize tinylength = tinystr_length(handle);
  hString handle_to_eat = handle;

  if (tinylength >= REQ_ALLOC_LENGTH) PANIC("%llx has improper tinylength %zu", (long long)handle, tinylength);

  /* string is stored backwards because it's easiest to just get the low byte */
  while (index < tinylength && index < max_length) {
    buffer[index] = (handle_to_eat & 0xFF); /* get lowest byte and move down */
    handle_to_eat >>= 8;
    index ++;
  }
  buffer[index] = '\0'; /* don't forget NUL! */
}

static hString tinystr_from_buffer(const char *buffer, usize length) {
  if (length >= REQ_ALLOC_LENGTH) PANIC("cannot create tinystr of length %zu", length);

  hString new_handle = 0;
  /* we encode the string backwards so it's easier to decode */
  for (int i = 0; i < length; i++) {
    new_handle <<= 8;
    new_handle |= buffer[length - i - 1];
  }

  /* set length in high byte */
  new_handle |= ((u64)length << (8 * 7));

  return new_handle;
}

static void set_entry_size(strentry *e, usize length) {
  if (e->is_external) {
    sbBuffer_set_size(&e->somewhere_else_value, length + 1);
    /* if we shortened string, need to replace NUL at end */
    e->length = length;
    e->somewhere_else_value.data[length] = '\0';
  } else {
    if (length > INLINE_BUFFER_SIZE - 1) {
      /* new length is too big for inline, so we need to copy to the external heap */
      sbBuffer new_buf = {0};
      /* length + 1 for NUL terminator. even though we track the length, it's
       * easier to just preserve NUL terminator for talking to C lib functions */
      sbBuffer_initialize(&new_buf, length + 1);
      sbBuffer_set_size(&new_buf, length + 1);
      if (e->length > 0) {
        memcpy(new_buf.data, e->inline_value, e->length + 1);
      }
      e->is_external = TRUE;
      e->somewhere_else_value = new_buf;
      e->length = length;
      e->somewhere_else_value.data[e->length] = '\0';
    } else {
      /* we can remain inline, but need to set NUL in the right place */
      e->length = length;
      e->inline_value[length] = '\0';
    }
  }
}

static void place_str_at_offset(strentry *e, usize offset, const char *str, usize length) {
  if (offset >= e->length) return; /* no room */

  usize max_bytes_to_write = e->length - offset;
  if (length < max_bytes_to_write) max_bytes_to_write = length;

  char *where_to_put;
  if (e->is_external) {
    where_to_put = &e->somewhere_else_value.data[offset];
  } else {
    where_to_put = &e->inline_value[offset];
  }

  memcpy(where_to_put, str, max_bytes_to_write);
  where_to_put[max_bytes_to_write] = '\0';
}

static strentry *new_entry(usize length) {
  usize index;
  strentry *new_entry = sbPool_alloc(&g_string_pool, &index);
  new_entry->handle = (index | FLAG_NONTINY);
  set_entry_size(new_entry, length);
  return new_entry;
}
