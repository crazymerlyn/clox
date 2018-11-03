#include <stdlib.h>

#include "chunk.h"
#include "memory.h"

void init_chunk(Chunk *chunk) {
    chunk->count = 0;
    chunk->capacity = 0;
    chunk->code = NULL;
}

void free_chunk(Chunk *chunk) {
    FREE_ARRAY(chunk->code, uint8_t, chunk->capacity);
    init_chunk(chunk);
}

void write_chunk(Chunk *chunk, uint8_t byte) {
    if (chunk->capacity < chunk->count + 1) {
        int oldCapacity = chunk->capacity;
        chunk->capacity = GROW_CAPACITY(oldCapacity);
        chunk->code = GROW_ARRAY(chunk->code, uint8_t, oldCapacity, chunk->capacity);
    }

    chunk->code[chunk->count] = byte;
    chunk->count++;
}
