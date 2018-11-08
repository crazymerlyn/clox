#include <stdio.h>

#include "debug.h"
#include "value.h"

static int simple_instruction(const char *name, int offset) {
    printf("%s\n", name);
    return offset + 1;
}

static int constant_instruction(const char *name, Chunk *chunk, int offset) {
    uint8_t index = chunk->code[offset + 1];
    printf("%-16s %4d '", name, index);
    print_value(chunk->constants.values[index]);
    printf("'\n");
    return offset + 2;
}

static int constant_long_instruction(const char *name, Chunk *chunk, int offset) {
    int index = (((int)chunk->code[offset + 1]) << 16) + (((int)chunk->code[offset + 2]) << 8) + chunk->code[offset + 3];
    printf("%-16s %4d '", name, index);
    print_value(chunk->constants.values[index]);
    printf("'\n");
    return offset + 4;
}

void disassemble_chunk(Chunk *chunk, const char *name) {
    printf("== %s ==\n", name);

    for (int i = 0; i < chunk->count;) {
        i = disassemble_instruction(chunk, i);
    }
}

int disassemble_instruction(Chunk *chunk, int offset) {
    printf("%04d ", offset);
    if (offset > 0 && chunk->lines[offset] == chunk->lines[offset - 1]) {
        printf("   | ");
    } else {
        printf("%4d ", chunk->lines[offset]);
    }

    uint8_t instruction = chunk->code[offset];
    switch(instruction) {
    case OP_CONSTANT:
        return constant_instruction("OP_CONSTANT", chunk, offset);
    case OP_CONSTANT_LONG:
        return constant_long_instruction("OP_CONSTANT_LONG", chunk, offset);
    case OP_RETURN:
        return simple_instruction("OP_RETURN", offset);
    default:
        printf("Unknown opcode %d\n", instruction);
        return offset + 1;
    }
}
