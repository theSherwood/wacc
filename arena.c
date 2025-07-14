#include <stdalign.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "compiler.h"

#define ARENA_BLOCK_CAPACITY 4096

#define ALIGN_UP(x, align) (((x) + ((align) - 1)) & ~((align) - 1))

typedef struct ArenaChunk {
  struct ArenaChunk* next;
  size_t capacity;
  size_t used;
  size_t mem[];
} ArenaChunk;

struct Arena {
  ArenaChunk* head;
};

Arena* arena_create() {
  Arena* arena = malloc(sizeof(Arena));
  arena->head = NULL;
  return arena;
}

void* arena_alloc(Arena* arena, size_t size) {
  if (arena->head == NULL || arena->head->used + size > arena->head->capacity) {
    size_t resolved_capacity = size < ARENA_BLOCK_CAPACITY ? ARENA_BLOCK_CAPACITY : size;
    size_t chunk_size = sizeof(ArenaChunk) + resolved_capacity;
    ArenaChunk* chunk = malloc(chunk_size);
    chunk->next = arena->head;
    chunk->used = 0;
    chunk->capacity = resolved_capacity;
    arena->head = chunk;
  }
  arena->head->used = ALIGN_UP(arena->head->used, alignof(void*));
  void* ptr = arena->head->mem + arena->head->used;
  arena->head->used += size;
  return ptr;
}

void arena_free(Arena* arena) {
  ArenaChunk* chunk = arena->head;
  while (chunk) {
    ArenaChunk* next = chunk->next;
    free(chunk);
    chunk = next;
  }
  arena->head = NULL;
}
