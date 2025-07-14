#include "compiler.h"
#include <stdlib.h>
#include <string.h>

typedef struct Arena {
    char* memory;
    size_t size;
    size_t used;
} Arena;

Arena* arena_create(size_t size) {
    Arena* arena = malloc(sizeof(Arena));
    if (!arena) return NULL;
    
    arena->memory = malloc(size);
    if (!arena->memory) {
        free(arena);
        return NULL;
    }
    
    arena->size = size;
    arena->used = 0;
    return arena;
}

void arena_destroy(Arena* arena) {
    if (!arena) return;
    free(arena->memory);
    free(arena);
}

void* arena_alloc(Arena* arena, size_t size) {
    if (!arena) return NULL;
    
    // Align to 8-byte boundary
    size = (size + 7) & ~7;
    
    if (arena->used + size > arena->size) {
        return NULL; // Out of memory
    }
    
    void* ptr = arena->memory + arena->used;
    arena->used += size;
    return ptr;
}

void arena_reset(Arena* arena) {
    if (arena) {
        arena->used = 0;
    }
}
