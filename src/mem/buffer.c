#include "buffer.h"

#include <string.h>

void sbBuffer_initialize(hBuffer buf, usize initial_size) {
    char *data = malloc(initial_size);

    if (data) {
        *buf = (sbBuffer) {
            .size = 0,
            .capacity = initial_size,
            .data = data,
        };
    } else {
        *buf = (sbBuffer) {0};
    }
}

void *sbBuffer_expand(hBuffer buf, usize expand_size) {
    if (expand_size == 0) return NULL;

    if (buf->capacity == 0 || buf->data == NULL) {
        fprintf(stderr, "cannot expand invalid buffer!\n");
        return NULL;
    }

    char *result;
    usize new_size = buf->size + expand_size;
    if (new_size > buf->capacity) {
        /* not enough space to fit requested chunk. reallocate to fit */
        usize new_capacity = buf->capacity * 3 / 2;
        if (new_capacity < new_size) {
            new_capacity = new_size;
        }

        char *new_data = realloc(buf->data, new_capacity);
        if (new_data) {
            buf->data = new_data;
        } else {
            fprintf(stderr, "failed to expand buffer!");
            return NULL;
        }
    }

    result = &buf->data[buf->size];
    buf->size = new_size;
    memset(result, 0, expand_size);
    return result;
}

void *sbBuffer_shrink(hBuffer buf, usize shrink_size) {
    if (shrink_size > buf->size) {
        fprintf(stderr, "internal warning: can't shrink buffer past zero");
        shrink_size = buf->size;
    }

    buf->size -= shrink_size;

    /* ok to return this even though it's "outside" the buffer,
     * because we never shrink the actual allocation of the buffer.
     * however, bear in mind not to keep this pointer around very
     * long, because if we append to the buffer it could change
     * again. */
    return &buf->data[buf->size];
}

void sbBuffer_set_size(hBuffer buf, usize new_size) {
  if (new_size > buf->size) {
    sbBuffer_expand(buf, new_size - buf->size);
  } else if (new_size < buf->size) {
    sbBuffer_shrink(buf, buf->size - new_size);
  }
}

void sbBuffer_append(hBuffer buf, const char *data, usize data_length) {
    if (data_length == 0) return;

    char *put = sbBuffer_expand(buf, data_length);
    memcpy(put, data, data_length);
}

void sbBuffer_reset(hBuffer buf) {
    buf->size = 0;
}

void sbBuffer_deinitialize(hBuffer buf) {
    free(buf->data);
    *buf = (sbBuffer) {0};
}
