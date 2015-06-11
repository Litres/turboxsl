#ifndef MEMORY_ALLOCATOR_H_
#define MEMORY_ALLOCATOR_H_

#include <pthread.h>

typedef struct memory_allocator_ memory_allocator;

typedef enum {
    MEMORY_ALLOCATOR_MODE_SELF = 0,
    MEMORY_ALLOCATOR_MODE_STYLESHEET,
    MEMORY_ALLOCATOR_MODE_GLOBAL
} MEMORY_ALLOCATOR_MODE;

memory_allocator *memory_allocator_create();

void memory_allocator_set_custom(memory_allocator *allocator, MEMORY_ALLOCATOR_MODE mode, memory_allocator *custom_allocator);

void memory_allocator_release(memory_allocator *allocator);

void memory_allocator_add_entry(memory_allocator *allocator, pthread_t thread, size_t size);

void memory_allocator_set_current(memory_allocator *allocator);

int memory_allocator_activate_mode(MEMORY_ALLOCATOR_MODE mode);

void *memory_allocator_new(size_t size);

#endif
