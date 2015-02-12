#ifndef NODE_CACHE_H_
#define NODE_CACHE_H_

#include <pthread.h>

typedef struct node_cache_ node_cache;

node_cache *node_cache_create();

void node_cache_release(node_cache *cache);

void node_cache_add_entry(node_cache *cache, pthread_t thread, size_t size);

void *node_cache_get(node_cache *cache);

#endif