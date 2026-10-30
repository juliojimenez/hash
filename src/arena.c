#include <stdlib.h>
#include <string.h>
#include "arena.h"

struct ArenaChunk {
    ArenaChunk *next;
    size_t capacity;
    size_t used;
    size_t pad;             /* Keeps data[] on a 16-byte boundary */
    unsigned char data[];
};

static size_t align_up(size_t size) {
    return (size + ARENA_ALIGNMENT - 1) & ~((size_t)ARENA_ALIGNMENT - 1);
}

static ArenaChunk *chunk_new(size_t capacity) {
    ArenaChunk *chunk = malloc(sizeof(ArenaChunk) + capacity);
    if (!chunk) {
        return NULL;
    }
    chunk->next = NULL;
    chunk->capacity = capacity;
    chunk->used = 0;
    chunk->pad = 0;
    return chunk;
}

void arena_init(Arena *arena, size_t chunk_size) {
    arena->current = NULL;
    arena->chunk_size = chunk_size ? align_up(chunk_size) : ARENA_DEFAULT_CHUNK_SIZE;
    arena->used = 0;
}

void *arena_alloc(Arena *arena, size_t size) {
    if (size == 0) {
        size = 1;
    }
    size = align_up(size);

    ArenaChunk *chunk = arena->current;
    if (!chunk || chunk->used + size > chunk->capacity) {
        size_t capacity = size > arena->chunk_size ? size : arena->chunk_size;
        chunk = chunk_new(capacity);
        if (!chunk) {
            return NULL;
        }
        chunk->next = arena->current;
        arena->current = chunk;
    }

    void *ptr = chunk->data + chunk->used;
    chunk->used += size;
    arena->used += size;
    return ptr;
}

char *arena_strndup(Arena *arena, const char *s, size_t n) {
    char *copy = arena_alloc(arena, n + 1);
    if (!copy) {
        return NULL;
    }
    memcpy(copy, s, n);
    copy[n] = '\0';
    return copy;
}

char *arena_strdup(Arena *arena, const char *s) {
    return arena_strndup(arena, s, strlen(s));
}

void arena_reset(Arena *arena) {
    ArenaChunk *keep = arena->current;
    if (!keep) {
        arena->used = 0;
        return;
    }

    /* Keep the most recent chunk warm, drop the rest */
    ArenaChunk *chunk = keep->next;
    while (chunk) {
        ArenaChunk *next = chunk->next;
        free(chunk);
        chunk = next;
    }
    keep->next = NULL;
    keep->used = 0;
    arena->used = 0;
}

void arena_free(Arena *arena) {
    ArenaChunk *chunk = arena->current;
    while (chunk) {
        ArenaChunk *next = chunk->next;
        free(chunk);
        chunk = next;
    }
    arena->current = NULL;
    arena->used = 0;
}
