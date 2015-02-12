#include "node_cache.h"

#include <stdio.h>

int main(int argc, char *argv[])
{
    node_cache *cache = node_cache_create();
    if (cache == NULL)
    {
        printf("ERROR: cache not created");
        return 0;
    }

    size_t size = 4;
    node_cache_add_entry(cache, pthread_self(), size);
    for (size_t i = 0; i < size; i++)
    {
        void *r = node_cache_get(cache);
        if (r == NULL)
        {
            printf("ERROR: no free node");
            return 0;
        }
    }

    void *r = node_cache_get(cache);
    if (r != NULL)
    {
        printf("ERROR: extra free node");
        return 0;
    }

    node_cache_release(cache);

    return 0;
}