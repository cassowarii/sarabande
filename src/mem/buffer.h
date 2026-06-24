#include "global.h"

/* Classic dynamic-sized array that expands when full.
 * Calls realloc(), so locations of pointers inside the array may shift in transit.
 * If you need a big block of memory that keeps consistent pointer values, use sbArena instead.
 * However, memory allocated from sbArena must be statically sized!
 * You can use sbBuffer to read in data you don't know the size of, then request a chunk
 * of that size from sbArena and copy it to there for long-term storage. */

typedef struct sbBuffer {
    usize size;
    usize capacity;
    char *data;
} sbBuffer;

typedef sbBuffer *hBuffer;

void sbBuffer_initialize(hBuffer buf, usize initial_size);

void *sbBuffer_expand(hBuffer buf, usize expand_size);

void *sbBuffer_shrink(hBuffer buf, usize shrink_size);

void sbBuffer_set_size(hBuffer buf, usize new_size);

void sbBuffer_append(hBuffer buf, const char *data, usize data_length);

void sbBuffer_reset(hBuffer buf);

void sbBuffer_deinitialize(hBuffer buf);
