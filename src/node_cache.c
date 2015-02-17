#include "node_cache.h"

#include <stdlib.h>
#include <string.h>

#include "logger.h"

typedef struct memory_cache_data {
    void *data;
    size_t data_size;
    size_t offset;
    struct memory_cache_data *next_entry;
} memory_cache_data;

typedef struct memory_cache_entry {
    pthread_t thread;
    memory_cache_data *head;
    memory_cache_data *tail;
    struct memory_cache_entry *next_entry;
} memory_cache_entry;

struct memory_cache_ {
    struct memory_cache_entry *entry;
};

memory_cache *global_cache = NULL;

memory_cache_data *memory_cache_create_data(size_t size)
{
    memory_cache_data *data = malloc(sizeof(memory_cache_data));
    if (data == NULL)
    {
        error("memory_cache_data:: malloc");
        return NULL;
    }
    memset(data, 0, sizeof(memory_cache_data));
    data->data_size = size;

    data->data = malloc(size);
    if (data->data == NULL)
    {
        error("memory_cache_data:: data malloc");
        return NULL;
    }
    memset(data->data, 0, size);

    return data;
}

void memory_cache_reset_data(memory_cache_data *data)
{
    while (data != NULL)
    {
        memset(data->data, 0, data->data_size);
        data->offset = 0;
        data = data->next_entry;
    }
}

int memory_cache_create()
{
    memory_cache *cache = malloc(sizeof(memory_cache));
    if (cache == NULL)
    {
        error("memory_cache_create:: malloc");
        return 1;
    }

    memset(cache, 0, sizeof(memory_cache));
    global_cache = cache;
    return 0;
}

void memory_cache_reset()
{
    memory_cache_entry *entry = global_cache->entry;
    while (entry != NULL)
    {
        memory_cache_reset_data(entry->head);
        entry = entry->next_entry;
    }
}

void memory_cache_release()
{
    memory_cache_entry *entry = global_cache->entry;
    while (entry != NULL)
    {
        memory_cache_entry *t = entry->next_entry;
        free(entry);
        entry = t;
    }
    free(global_cache);
}

void memory_cache_add_entry(pthread_t thread, size_t size)
{
    memory_cache_entry *e = malloc(sizeof(memory_cache_entry));
    if (e == NULL)
    {
        error("memory_cache_add_entry:: entry NULL");
        return;
    }

    memset(e, 0, sizeof(memory_cache_entry));
    e->thread = thread;
    e->head = memory_cache_create_data(size);
    if (e->head == NULL)
    {
        error("memory_cache_add_entry:: data NULL");
        return;
    }
    e->tail = e->head;

    if (global_cache->entry == NULL)
    {
        global_cache->entry = e;
    }
    else
    {
        memory_cache_entry *t = global_cache->entry;
        while (t->next_entry != NULL) t = t->next_entry;
        t->next_entry = e;
    }
}

void *memory_cache_allocate(size_t size)
{
    pthread_t self = pthread_self();
    memory_cache_entry *t = global_cache->entry;
    while (t != NULL && t->thread != self) t = t->next_entry;
    if (t == NULL || t->thread != self)
    {
        error("memory_cache_allocate:: unknown thread");
        return NULL;
    }

    memory_cache_data *data = t->tail;
    if (data->offset + size > data->data_size)
    {
        debug("memory_cache_allocate:: data entry full");
        if (data->next_entry == NULL)
        {
            memory_cache_data *new_data = memory_cache_create_data(data->data_size);
            if (new_data == NULL) return NULL;

            data->next_entry = new_data;
            t->tail = new_data;

            data = new_data;
        }
        else
        {
            data = data->next_entry;
        }
    }

    void *result = data->data + data->offset;
    data->offset = data->offset + size;

    return result;
}
