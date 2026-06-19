#include "arena.h"

#include <string.h>

#define ALIGN 8

struct block {
    struct block *next;
    usize used;
    usize capacity;
    char data[];
};

struct sbArena {
    struct block *first;
    struct block *current;
    struct block *last;
};

hArena sbArena_create(usize initial_size) {
    hArena arena = malloc(sizeof(struct sbArena));

    while (initial_size % ALIGN != 0) initial_size++;

    struct block *block = calloc(sizeof(struct block) + initial_size, 1);
    block->used = 0;
    block->next = NULL;
    block->capacity = initial_size;

    arena->first = block;
    arena->current = block;
    arena->last = block;

    return arena;
}

void *sbArena_alloc(hArena arena, usize size) {
    while (size % ALIGN != 0) size++;

    if (arena->current->used > arena->current->capacity) {
        PANIC("somehow arena values are out of sync");
    }

    while (arena->current != arena->last && size > arena->current->capacity - arena->current->used) {
        /* move to next block until we find one that fits */
        if (arena->current->next) {
            arena->current = arena->current->next;
        } else {
            PANIC("lost track of memory block in arena somehow; should not happen");
        }
    }

    if (size > arena->current->capacity - arena->current->used) {
        /* if still doesn't fit, we must be at the end, need to allocate a new block */
        usize new_capacity = arena->current->capacity;
        if (size > new_capacity) new_capacity = size;
        struct block *block = calloc(sizeof(struct block) + new_capacity, 1);
        block->capacity = new_capacity;
        block->next = NULL;

        arena->current->next = block;
        arena->current = arena->last = block;
    }

    void *allocated_ptr = &arena->current->data[arena->current->used];
    arena->current->used += size;
    memset(allocated_ptr, 0, size);

    return allocated_ptr;
}

void sbArena_reset(hArena arena) {
    struct block *blk = arena->first;
    do {
        blk->used = 0;
    } while (blk != arena->current);

    arena->current = arena->first;
}

void sbArena_destroy(hArena arena) {
    struct block *blk = arena->first;
    struct block *blk_next = arena->first->next;
    do {
        blk_next = blk->next;
        free(blk);
        blk = blk_next;
    } while (blk);

    free(arena);
}
