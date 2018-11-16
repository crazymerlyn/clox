#include <stdio.h>
#include <string.h>

#include "memory.h"
#include "object.h"
#include "value.h"
#include "vm.h"

#define ALLOCATE_OBJ(vm, type, objectType) \
    (type*) allocate_object(vm, sizeof(type), objectType)

static Obj *allocate_object(VM *vm, size_t size, ObjType type) {
    Obj *obj = (Obj *)reallocate(NULL, 0, size);
    obj->type = type;

    obj->next = vm->objects;
    vm->objects = obj;
    return obj;
}

ObjString *allocate_string(VM *vm, char *chars, int length) {
    ObjString *string = ALLOCATE_OBJ(vm, ObjString, OBJ_STRING);
    string->length = length;
    string->chars = chars;

    return string;
}

ObjString *take_string(VM *vm, char *chars, int length) {
    return allocate_string(vm, chars, length);
}

ObjString *copy_string(VM *vm, const char *chars, int length) {
    char *heap_chars = ALLOCATE(char, length + 1);
    memcpy(heap_chars, chars, length);
    heap_chars[length] = '\0';

    return allocate_string(vm, heap_chars, length);
}
