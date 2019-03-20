#include <stdlib.h>

#include "chunk.h"
#include "memory.h"

void init_chunk(Chunk *chunk) {
    chunk->count = 0;
    chunk->capacity = 0;
    chunk->code = NULL;
    chunk->lines = NULL;
    init_value_array(&chunk->constants);
}

void free_chunk(Chunk *chunk) {
    FREE_ARRAY(chunk->code, uint8_t, chunk->capacity);
    FREE_ARRAY(chunk->lines, uint8_t, chunk->capacity);
    free_value_array(&chunk->constants);
    init_chunk(chunk);
}

void write_chunk(Chunk *chunk, uint8_t byte, int line) {
    if (chunk->capacity < chunk->count + 1) {
        int oldCapacity = chunk->capacity;
        chunk->capacity = GROW_CAPACITY(oldCapacity);
        chunk->code = GROW_ARRAY(chunk->code, uint8_t, oldCapacity, chunk->capacity);
        chunk->lines = GROW_ARRAY(chunk->lines, int, oldCapacity, chunk->capacity);
    }

    chunk->code[chunk->count] = byte;
    chunk->lines[chunk->count] = line;
    chunk->count++;
}

int write_constant(Chunk *chunk, Value value, int line) {
    int index = add_constant(chunk, value);
    if (index <= 0xff) {
        write_chunk(chunk, OP_CONSTANT, line);
        write_chunk(chunk, index, line);
    } else {
        write_chunk(chunk, OP_CONSTANT_LONG, line);
        write_chunk(chunk, index >> 16, line);
        write_chunk(chunk, (index >> 8) & 0xff, line);
        write_chunk(chunk, index & 0xff, line);
    }
    return index;
}

int add_constant(Chunk *chunk, Value value) {
    write_value_array(&chunk->constants, value);
    return chunk->constants.count - 1;
}
