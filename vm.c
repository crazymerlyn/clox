#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "common.h"
#include "memory.h"
#include "compiler.h"
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
    vm->objects = NULL;
    init_table(&vm->globals);
    init_table(&vm->strings);
}

void free_vm(VM *vm) {
    free_table(&vm->globals);
    free_table(&vm->strings);
    free_objects(vm->objects);
}

static Value peek(VM *vm, int dist) {
    return vm->stack_top[-1 - dist];
}

static void runtime_error(VM *vm, const char *format, ...) {
    va_list args;
    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);
    fputs("\n", stderr);

    size_t inst = vm->ip - vm->chunk->code;
    fprintf(stderr, "[line %d] in script\n",
            vm->chunk->lines[inst]);
}

static bool is_falsey(Value value) {
    return IS_NIL(value) || (IS_BOOL(value) && !AS_BOOL(value));
}

static void concatenate(VM *vm) {
    ObjString *b = AS_STRING(pop(vm));
    ObjString *a = AS_STRING(pop(vm));

    int length = a->length + b->length;
    char *chars = ALLOCATE(char, length + 1);
    memcpy(chars, a->chars, a->length);
    memcpy(chars + a->length, b->chars, b->length);
    chars[length] = '\0';

    ObjString *result = take_string(vm, chars, length);
    push(vm, OBJ_VAL(result));
}

static InterpretResult run(VM *vm) {
#define READ_BYTE() (*vm->ip++)
#define READ_CONSTANT() (vm->chunk->constants.values[READ_BYTE()])
#define READ_STRING() AS_STRING(READ_CONSTANT())

#define BINARY_OP(value_type, op) \
    do { \
        if (!IS_NUMBER(peek(vm, 0)) || !IS_NUMBER(peek(vm, 1))) { \
            runtime_error(vm, "Operands must be numbers."); \
            return INTERPRET_RUNTIME_ERROR; \
        } \
        double b = AS_NUMBER(pop(vm)); \
        double a = AS_NUMBER(pop(vm)); \
        push(vm, value_type(a op b)); \
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
        case OP_NIL: push(vm, NIL_VAL); break;
        case OP_TRUE: push(vm, BOOL_VAL(true)); break;
        case OP_FALSE: push(vm, BOOL_VAL(false)); break;
        case OP_POP: pop(vm); break;
        case OP_GET_GLOBAL: {
            ObjString *name = READ_STRING();
            Value value;
            if (!table_get(&vm->globals, name, &value)) {
                runtime_error(vm, "Undefined variable '%s'.", name->chars);
                return INTERPRET_RUNTIME_ERROR;
            }
            push(vm, value);
            break;
        }
        case OP_DEFINE_GLOBAL: {
            ObjString *name = READ_STRING();
            table_set(&vm->globals, name, peek(vm, 0));
            pop(vm);
            break;
        }
        case OP_SET_GLOBAL: {
            ObjString *name = READ_STRING();
            if (table_set(&vm->globals, name, peek(vm, 0))) {
                runtime_error(vm, "Undefined variable '%s'.", name->chars);
                return INTERPRET_RUNTIME_ERROR;
            }
            break;
        }
        case OP_EQUAL: {
            Value b = pop(vm);
            Value a = pop(vm);
            push(vm, BOOL_VAL(values_equal(a, b)));
            break;
        }
        case OP_GREATER:    BINARY_OP(BOOL_VAL, >); break;
        case OP_LESS:       BINARY_OP(BOOL_VAL, <); break;
        case OP_ADD: {
            if (IS_STRING(peek(vm, 0)) && IS_STRING(peek(vm, 1))) {
                concatenate(vm);
            } else if (IS_NUMBER(peek(vm, 0)) && IS_NUMBER(peek(vm, 1))) {
                double b = AS_NUMBER(pop(vm));
                double a = AS_NUMBER(pop(vm));
                push(vm, NUMBER_VAL(a + b));
            } else {
                runtime_error(vm, "Operands must be two numbers or two strings.");
                return INTERPRET_RUNTIME_ERROR;
            }
            break;
        }
        case OP_SUBTRACT:   BINARY_OP(NUMBER_VAL, -); break;
        case OP_MULTIPLY:   BINARY_OP(NUMBER_VAL, *); break;
        case OP_DIVIDE:     BINARY_OP(NUMBER_VAL, /); break;
        case OP_NOT:
            push(vm, BOOL_VAL(is_falsey(pop(vm)))); break;
        case OP_NEGATE:
            if (!IS_NUMBER(peek(vm, 0))) {
                runtime_error(vm, "Operand must be a number.");
                return INTERPRET_RUNTIME_ERROR;
            }
            push(vm, NUMBER_VAL(-AS_NUMBER(pop(vm)))); break;
        case OP_PRINT:
            print_value(pop(vm));
            printf("\n");
            break;
        case OP_RETURN:
            return INTERPRET_OK;
        }
    }

#undef READ_BYTE
#undef READ_CONSTANT
#undef READ_STRING
}

InterpretResult interpret(VM *vm, const char *source) {
    Chunk chunk;
    init_chunk(&chunk);
    if (!compile(vm, source, &chunk)) {
        free_chunk(&chunk);
        return INTERPRET_COMPILE_ERROR;
    }

    vm->chunk = &chunk;
    vm->ip = vm->chunk->code;

    InterpretResult result = run(vm);
    free_chunk(&chunk);
    return result;
}
