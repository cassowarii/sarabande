#include "common.h"

typedef struct sbArena *hArena;

hArena sbArena_create(usize initial_size);

void *sbArena_alloc(hArena arena, usize size);

void sbArena_reset(hArena arena);

void sbArena_destroy(hArena arena);
