#include "global.h"

/* Memory arena. Allocate memory in large chunks and hand out chunks of a requested size.
 * All memory in arena is reclaimed at once, and can be reused.
 * Position of memory blocks is fixed, so pointers to items in arena remain valid no matter what.
 * However, memory must be requested in fixed-size chunks and can't be reallocated.
 * For a flexible-length contiguous buffer that may need to grow in length, use sbBuffer instead.
 * (however, sbBuffer doesn't have the 'pointer validity' guarantee.) */

typedef struct sbArena {
    struct block *first;
    struct block *current;
    struct block *last;
} sbArena;

typedef struct sbArena *hArena;

void sbArena_initialize(hArena arena, usize initial_size);

void *sbArena_alloc(hArena arena, usize size);

void sbArena_reset(hArena arena);

void sbArena_destroy(hArena arena);
