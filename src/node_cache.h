#ifndef MEMORY_CACHE_H_
#define MEMORY_CACHE_H_

#include <pthread.h>

typedef struct memory_cache_ memory_cache;

memory_cache *memory_cache_create();

void memory_cache_release(memory_cache *cache);

void memory_cache_add_entry(memory_cache *cache, pthread_t thread, size_t size);

void memory_cache_set_current(memory_cache *cache);

void *memory_cache_allocate(size_t size);

#endif
