#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdint.h>

typedef struct {
    uint8_t *base; // start of the memory block
    size_t   used; // bytes handed out so far
    size_t   cap;  // total capacity in bytes
} Arena;

int   arena_init(Arena *a, size_t capacity); // allocates backing memory, returns -1 on failure
void *arena_alloc(Arena *a, size_t size);    // bump-pointer alloc, returns NULL if full
void  arena_reset(Arena *a);                 // resets used to 0 — reclaims all at once
void  arena_destroy(Arena *a);               // frees backing memory

#ifdef __cplusplus
}
#endif