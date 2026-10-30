#ifndef ARENA_H
#define ARENA_H

#include <stddef.h>

/**
 * Arena (bump) allocator for memory that lives exactly as long as one unit of
 * work, such as parsing and executing a single top-level command.
 *
 * Allocations are cheap and never freed individually: arena_reset() returns
 * everything in one step and keeps the current chunk for reuse, arena_free()
 * releases it all. Anything that must outlive the unit of work (function
 * bodies, aliases, variable values) has to be copied out first.
 */

typedef struct ArenaChunk ArenaChunk;

typedef struct {
    ArenaChunk *current;    /* Chunk allocations are served from (list head) */
    size_t chunk_size;      /* Default usable size of a new chunk */
    size_t used;            /* Bytes handed out since the last reset */
} Arena;

#define ARENA_DEFAULT_CHUNK_SIZE 4096
#define ARENA_ALIGNMENT 16

/**
 * Initialize an arena. chunk_size 0 selects ARENA_DEFAULT_CHUNK_SIZE.
 */
void arena_init(Arena *arena, size_t chunk_size);

/**
 * Allocate size bytes, aligned to ARENA_ALIGNMENT. Returns NULL only when the
 * underlying malloc fails. Requests larger than the chunk size get a chunk of
 * their own.
 */
void *arena_alloc(Arena *arena, size_t size);

/**
 * Copy n bytes of s into the arena and NUL-terminate the copy.
 */
char *arena_strndup(Arena *arena, const char *s, size_t n);

/**
 * Copy the NUL-terminated string s into the arena.
 */
char *arena_strdup(Arena *arena, const char *s);

/**
 * Release every allocation at once, keeping the current chunk for reuse.
 */
void arena_reset(Arena *arena);

/**
 * Release every chunk. The arena must be re-initialized before further use.
 */
void arena_free(Arena *arena);

#endif
