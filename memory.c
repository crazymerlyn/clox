#include <stdlib.h>

#include "common.h"
#include "memory.h"

void *reallocate(void *previous, size_t oldsize, size_t newsize) {
    if (newsize == 0) {
        free(previous); return NULL;
    }

    return realloc(previous, newsize);
}

static void free_object(Obj *obj) {
    switch(obj->type) {
        case OBJ_STRING: {
            ObjString *str = (ObjString*)obj;
            FREE_ARRAY(str->chars, char, str->length + 1);
            FREE(ObjString, obj);
            break;
        }
    }
}

void free_objects(Obj *obj) {
    while (obj != NULL) {
        Obj *next = obj->next;
        free_object(obj);
        obj = next;
    }
}
