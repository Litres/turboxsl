#ifndef MEMORY_CACHE_H_
#define MEMORY_CACHE_H_

#include <pthread.h>

typedef struct memory_cache_ memory_cache;

int memory_cache_create();

void memory_cache_reset();

void memory_cache_release();

void memory_cache_add_entry(pthread_t thread, size_t size);

void *memory_cache_allocate(size_t size);

#endif
