#include "arena.h"
#include <stdlib.h>
#include <string.h>

int arena_init(Arena *a, size_t capacity) {
    a->base = (uint8_t *)malloc(capacity);
    if (!a->base) return -1;
    a->used = 0;
    a->cap  = capacity;
    return 0;
}

void *arena_alloc(Arena *a, size_t size) {
    // align to 8 bytes — safe for all primitive types on ARM64
    size_t aligned = (size + 7) & ~(size_t)7;
    if (a->used + aligned > a->cap) return NULL;
    void *ptr = a->base + a->used;
    a->used  += aligned;
    return ptr;
}

void arena_reset(Arena *a) {
    a->used = 0; // reclaim all — old data remains but will be overwritten
}

void arena_destroy(Arena *a) {
    free(a->base);
    a->base = NULL;
    a->used = 0;
    a->cap  = 0;
}