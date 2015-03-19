#include "allocator.h"

#include <stdio.h>

int main(int argc, char *argv[])
{
    memory_allocator *allocator = memory_allocator_create();
    if (allocator == NULL)
    {
        printf("ERROR: allocator not created");
        return 0;
    }

    size_t size = 256;
    size_t object_size = 16;
    memory_allocator_add_entry(allocator, pthread_self(), size);
    memory_allocator_set_current(allocator);

    for (size_t i = 0; i < size / object_size; i++)
    {
        void *r = memory_allocator_new(object_size);
        if (r == NULL)
        {
            printf("ERROR: no free memory");
            return 0;
        }
    }

    void *r = memory_allocator_new(object_size);
    if (r != NULL)
    {
        printf("ERROR: extra free memory");
        return 0;
    }

    memory_allocator_release(allocator);

    return 0;
}