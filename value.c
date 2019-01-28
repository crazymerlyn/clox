#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "value.h"
#include "object.h"
#include "memory.h"

void init_value_array(ValueArray *value_array) {
    value_array->count = 0;
    value_array->capacity = 0;
    value_array->values = NULL;
}

void free_value_array(ValueArray *value_array) {
    FREE_ARRAY(value_array->values, uint8_t, value_array->capacity);
    init_value_array(value_array);
}

void write_value_array(ValueArray *value_array, Value value) {
    if (value_array->capacity < value_array->count + 1) {
        int oldCapacity = value_array->capacity;
        value_array->capacity = GROW_CAPACITY(oldCapacity);
        value_array->values = GROW_ARRAY(value_array->values, Value, oldCapacity, value_array->capacity);
    }

    value_array->values[value_array->count] = value;
    value_array->count++;
}

void print_object(Value value) {
    switch(OBJ_TYPE(value)) {
    case OBJ_STRING:
        printf("%s", AS_CSTRING(value));
    }
}

void print_value(Value value) {
    switch(value.type) {
    case VAL_BOOL: printf(AS_BOOL(value) ? "true" : "false"); break;
    case VAL_NIL: printf("nil"); break;
    case VAL_NUMBER: printf("%g", AS_NUMBER(value)); break;
    case VAL_OBJ: print_object(value); break;
    }
}

bool values_equal(Value a, Value b) {
    if (a.type != b.type) return false;
    switch(a.type) {
    case VAL_BOOL: return AS_BOOL(a) == AS_BOOL(b);
    case VAL_NIL: return true;
    case VAL_NUMBER: return AS_NUMBER(a) == AS_NUMBER(b);
    case VAL_OBJ: return AS_OBJ(a) == AS_OBJ(b);
    }
}

