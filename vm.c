#include <stdio.h>

#include "common.h"
#include "debug.h"
#include "vm.h"

static void reset_stack(VM *vm) {
    vm->stack_top = vm->stack;
}

void push(VM *vm, Value value) {
    *vm->stack_top = value;
    vm->stack_top++;
}

Value pop(VM *vm) {
    vm->stack_top--;
    return *vm->stack_top;
}

void init_vm(VM *vm) {
    reset_stack(vm);
}

void free_vm(VM *vm) {
}

static InterpretResult run(VM *vm) {
#define READ_BYTE() (*vm->ip++)
#define READ_CONSTANT() (vm->chunk->constants.values[READ_BYTE()])

#define BINARY_OP(op) \
    do { \
        Value b = pop(vm); \
        Value a = pop(vm); \
        push(vm, a op b); \
    } while (false)

    for (;;) {
#ifdef DEBUG_TRACE_EXECUTION
        printf("          ");
        for (Value *slot = vm->stack; slot < vm->stack_top; ++slot) {
            printf("[ ");
            print_value(*slot);
            printf(" ]");
        }
        printf("\n");
        disassemble_instruction(vm->chunk, (int)(vm->ip - vm->chunk->code));
#endif
        uint8_t instruction;
        switch (instruction = READ_BYTE()) {
        case OP_CONSTANT: {
            Value constant = READ_CONSTANT();
            push(vm, constant);
            break;
        }
        case OP_CONSTANT_LONG: {
            int index = *vm->ip++;
            index = index * 256 + *vm->ip++;
            index = index * 256 + *vm->ip++;
            Value constant = vm->chunk->constants.values[index];
            push(vm, constant);
            break;
        }
        case OP_ADD: BINARY_OP(+); break;
        case OP_SUBTRACT: BINARY_OP(-); break;
        case OP_MULTIPLY: BINARY_OP(*); break;
        case OP_DIVIDE: BINARY_OP(/); break;
        case OP_NEGATE: push(vm, -pop(vm)); break;
        case OP_RETURN:
            print_value(pop(vm));
            printf("\n");
            return INTERPRET_OK;
        }
    }

#undef READ_BYTE
#undef READ_CONSTANT
}

InterpretResult interpret(VM *vm, Chunk *chunk) {
    vm->chunk = chunk;
    vm->ip = vm->chunk->code;
    return run(vm);
}
