#include "common.h"
#include "chunk.h"
#include "debug.h"

int main(int argc, const char *argv[]) {
    Chunk chunk;
    init_chunk(&chunk);
    write_constant(&chunk, 1.2, 123);
    write_chunk(&chunk, OP_RETURN, 123);

    disassemble_chunk(&chunk, "test chunk");

    free_chunk(&chunk);
    return 0;
}
