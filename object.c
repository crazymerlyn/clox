#include <stdio.h>
#include <string.h>

#include "memory.h"
#include "object.h"
#include "value.h"
#include "vm.h"

#define ALLOCATE_OBJ(type, objectType) \
    (type*) allocate_object(sizeof(type), objectType)

static Obj *allocate_object(size_t size, ObjType type) {
    Obj *obj = (Obj *)reallocate(NULL, 0, size);
    obj->type = type;
    return obj;
}

ObjString *allocate_string(char *chars, int length) {
    ObjString *string = ALLOCATE_OBJ(ObjString, OBJ_STRING);
    string->length = length;
    string->chars = chars;

    return string;
}

ObjString *take_string(char *chars, int length) {
    return allocate_string(chars, length);
}

ObjString *copy_string(const char *chars, int length) {
    char *heap_chars = ALLOCATE(char, length + 1);
    memcpy(heap_chars, chars, length);
    heap_chars[length] = '\0';

    return allocate_string(heap_chars, length);
}
