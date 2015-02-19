#include "node_cache.h"

#include <stdlib.h>
#include <string.h>

#include "logger.h"

typedef struct memory_cache_data {
    void *area;
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

memory_cache *current_cache = NULL;

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

    data->area = malloc(size);
    if (data->area == NULL)
    {
        error("memory_cache_data:: data malloc");
        return NULL;
    }
    memset(data->area, 0, size);

    return data;
}

void memory_cache_release_data(memory_cache_data *data)
{
    size_t total_data_size = 0;
    memory_cache_data *current = data;
    while (current != NULL)
    {
        memory_cache_data *next = current->next_entry;
        total_data_size = total_data_size + current->data_size;
        free(current->area);
        free(current);
        current = next;
    }
    info("memory_cache_release_data:: free memory: %lu bytes", total_data_size);
}

size_t memory_cache_entry_size(memory_cache_entry *entry)
{
    size_t result = 0;
    memory_cache_data *current = entry->head;
    while (current != NULL)
    {
        result = result + current->data_size;
        current = current->next_entry;
    }
    return result;
}

size_t memory_cache_size(memory_cache *cache)
{
    size_t result = 0;
    memory_cache_entry *current = cache->entry;
    while (current != NULL)
    {
        result = result + memory_cache_entry_size(current);
        current = current->next_entry;
    }
    return result;
}

memory_cache *memory_cache_create()
{
    memory_cache *cache = malloc(sizeof(memory_cache));
    if (cache == NULL)
    {
        error("memory_cache_create:: malloc");
        return NULL;
    }

    memset(cache, 0, sizeof(memory_cache));
    return cache;
}

void memory_cache_release(memory_cache *cache)
{
    info("memory_cache_release:: cache size: %lu bytes", memory_cache_size(cache));
    memory_cache_entry *current = cache->entry;
    while (current != NULL)
    {
        memory_cache_entry *next = current->next_entry;
        memory_cache_release_data(current->head);
        free(current);
        current = next;
    }
    free(cache);
}

void memory_cache_add_entry(memory_cache *cache, pthread_t thread, size_t size)
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

    if (cache->entry == NULL)
    {
        cache->entry = e;
    }
    else
    {
        memory_cache_entry *t = cache->entry;
        while (t->next_entry != NULL) t = t->next_entry;
        t->next_entry = e;
    }
}

void memory_cache_set_current(memory_cache *cache)
{
    current_cache = cache;
}

void *memory_cache_allocate(size_t size)
{
    pthread_t self = pthread_self();
    memory_cache_entry *t = current_cache->entry;
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

    void *result = data->area + data->offset;
    data->offset = data->offset + size;

    return result;
}
