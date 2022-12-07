#ifndef dis_memory_h
#define dis_memory_h

#include "header.h"

#define ALLOCATE(type, count) \
    (type*)reallocate(NULL, 0, sizeof(type) * (count))

#define EXPAND_LIMITS(capacity) \
    ((capacity) < 8 ? 8 : (capacity * 2))

#define FREE_ARRAY(type, handle, count) \
    reallocate(handle, sizeof(type) * count, 0)

#define EXPAND_ARRAY(type, handle, limit, target_limit) \
    (type*)reallocate(handle, sizeof(type) * (limit), \
            sizeof(type) * (target_limit))

void* reallocate(void* pointer, size_t prev_size, size_t new_size);

#endif
