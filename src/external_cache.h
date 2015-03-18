#ifndef EXTERNAL_CACHE_H_
#define EXTERNAL_CACHE_H_

#include <pthread.h>

typedef struct external_cache_ external_cache;

external_cache *external_cache_create(char *server_list);
void external_cache_add_client(external_cache *cache, pthread_t thread);
void external_cache_release(external_cache *cache);

int external_cache_set(external_cache *cache, char *key, char *value);
char *external_cache_get(external_cache *cache, char *key);

#endif
