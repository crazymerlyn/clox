#ifndef clox_memory_h
#define clox_memory_h

#define ALLOCATE(type, count) \
    (type*) reallocate(NULL, 0, sizeof(type) * (count))

#define GROW_CAPACITY(capacity) \
    ((capacity) < 8 ? 8 : (capacity) * 2)

#define GROW_ARRAY(previous, type, oldCount, count) \
    (type*) reallocate(previous, sizeof(type) * oldCount, \
        sizeof(type) * count)

#define FREE_ARRAY(pointer, type, count) \
    reallocate(pointer, sizeof(type) * (count), 0)

void *reallocate(void *previous, size_t oldsize, size_t newsize);

#endif
