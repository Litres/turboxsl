#include "node_cache.h"

#include <stdio.h>

int main(int argc, char *argv[])
{
    memory_cache *cache = memory_cache_create();
    if (cache == NULL)
    {
        printf("ERROR: cache not created");
        return 0;
    }

    size_t size = 256;
    size_t object_size = 16;
    memory_cache_add_entry(cache, pthread_self(), size);
    memory_cache_set_current(cache);

    for (size_t i = 0; i < size / object_size; i++)
    {
        void *r = memory_cache_allocate(object_size);
        if (r == NULL)
        {
            printf("ERROR: no free memory");
            return 0;
        }
    }

    void *r = memory_cache_allocate(object_size);
    if (r != NULL)
    {
        printf("ERROR: extra free memory");
        return 0;
    }

    memory_cache_release(cache);

    return 0;
}