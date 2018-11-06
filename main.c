#include "common.h"
#include "chunk.h"
#include "debug.h"

int main(int argc, const char *argv[]) {
    Chunk chunk;
    init_chunk(&chunk);
    int index = add_constant(&chunk, 1.2);
    write_chunk(&chunk, OP_CONSTANT);

    write_chunk(&chunk, index);
    write_chunk(&chunk, OP_RETURN);

    disassemble_chunk(&chunk, "test chunk");

    free_chunk(&chunk);
    return 0;
}
