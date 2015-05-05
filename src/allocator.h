#ifndef MEMORY_ALLOCATOR_H_
#define MEMORY_ALLOCATOR_H_

#include <pthread.h>

typedef struct memory_allocator_ memory_allocator;

memory_allocator *memory_allocator_create(memory_allocator *parent);

void memory_allocator_release(memory_allocator *allocator);

void memory_allocator_add_entry(memory_allocator *allocator, pthread_t thread, size_t size);

void memory_allocator_set_current(memory_allocator *allocator);

int memory_allocator_activate_parent(int activate);

void *memory_allocator_new(size_t size);

#endif
