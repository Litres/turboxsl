#include "node_cache.h"

#include <stdio.h>

int main(int argc, char *argv[])
{
    if (memory_cache_create() != 0)
    {
        printf("ERROR: cache not created");
        return 0;
    }

    size_t size = 256;
    size_t object_size = 16;
    memory_cache_add_entry(pthread_self(), size);
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

    memory_cache_release();

    return 0;
}