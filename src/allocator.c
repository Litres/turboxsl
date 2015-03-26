#include "allocator.h"

#include <stdlib.h>
#include <string.h>

#include "logger.h"

typedef struct memory_allocator_data {
    void *area;
    size_t data_size;
    size_t offset;
    struct memory_allocator_data *next_entry;
} memory_allocator_data;

typedef struct memory_allocator_entry {
    pthread_t thread;
    int is_actual;
    memory_allocator_data *head;
    memory_allocator_data *tail;
    struct memory_allocator_entry *next_entry;
} memory_allocator_entry;

struct memory_allocator_ {
    memory_allocator *parent_allocator;
    struct memory_allocator_entry *entry;
};

memory_allocator *current_allocator = NULL;

memory_allocator_data *memory_allocator_create_data(size_t size)
{
    memory_allocator_data *data = malloc(sizeof(memory_allocator_data));
    if (data == NULL)
    {
        error("memory_allocator_data:: malloc");
        return NULL;
    }
    memset(data, 0, sizeof(memory_allocator_data));
    data->data_size = size;

    data->area = malloc(size);
    if (data->area == NULL)
    {
        error("memory_allocator_data:: data malloc");
        return NULL;
    }
    memset(data->area, 0, size);

    return data;
}

void memory_cache_release_data(memory_allocator_data *data)
{
    size_t total_data_size = 0;
    memory_allocator_data *current = data;
    while (current != NULL)
    {
        memory_allocator_data *next = current->next_entry;
        total_data_size = total_data_size + current->data_size;
        free(current->area);
        free(current);
        current = next;
    }
    debug("memory_allocator_release_data:: free memory: %lu bytes", total_data_size);
}

size_t memory_allocator_entry_size(memory_allocator_entry *entry)
{
    size_t result = 0;
    memory_allocator_data *current = entry->head;
    while (current != NULL)
    {
        result = result + current->data_size;
        current = current->next_entry;
    }
    return result;
}

size_t memory_allocator_size(memory_allocator *allocator)
{
    size_t result = 0;
    memory_allocator_entry *current = allocator->entry;
    while (current != NULL)
    {
        result = result + memory_allocator_entry_size(current);
        current = current->next_entry;
    }
    return result;
}

memory_allocator *memory_allocator_create()
{
    memory_allocator *allocator = malloc(sizeof(memory_allocator));
    if (allocator == NULL)
    {
        error("memory_allocator_create:: malloc");
        return NULL;
    }

    memset(allocator, 0, sizeof(memory_allocator));
    return allocator;
}

void memory_allocator_release(memory_allocator *allocator)
{
    debug("memory_allocator_release:: total size: %lu bytes", memory_allocator_size(allocator));
    memory_allocator_entry *current = allocator->entry;
    while (current != NULL)
    {
        memory_allocator_entry *next = current->next_entry;
        memory_cache_release_data(current->head);
        free(current);
        current = next;
    }
    free(allocator);
}

void memory_allocator_add_entry(memory_allocator *allocator, pthread_t thread, size_t size)
{
    memory_allocator_entry *e = malloc(sizeof(memory_allocator_entry));
    if (e == NULL)
    {
        error("memory_allocator_add_entry:: entry NULL");
        return;
    }

    memset(e, 0, sizeof(memory_allocator_entry));
    e->thread = thread;
    e->is_actual = 1;
    e->head = memory_allocator_create_data(size);
    if (e->head == NULL)
    {
        error("memory_allocator_add_entry:: data NULL");
        return;
    }
    e->tail = e->head;

    if (allocator->entry == NULL)
    {
        allocator->entry = e;
    }
    else
    {
        memory_allocator_entry *t = allocator->entry;
        while (t->next_entry != NULL) t = t->next_entry;
        t->next_entry = e;
    }
}

void memory_allocator_set_current(memory_allocator *allocator)
{
    current_allocator = allocator;
}

void memory_allocator_set_parent(memory_allocator *allocator, memory_allocator *parent)
{
    allocator->parent_allocator = parent;
}

void memory_allocator_activate_parent(int activate)
{
    pthread_t self = pthread_self();
    memory_allocator_entry *t = current_allocator->entry;
    while (t != NULL && t->thread != self) t = t->next_entry;
    if (t == NULL || t->thread != self)
    {
        error("memory_allocator_activate_parent:: unknown thread");
        return;
    }
    t->is_actual = !activate;
}

void *memory_allocator_new(size_t size)
{
    pthread_t self = pthread_self();
    memory_allocator_entry *t = current_allocator->entry;
    while (t != NULL && t->thread != self) t = t->next_entry;
    if (t == NULL || t->thread != self)
    {
        error("memory_allocator_new:: unknown thread");
        return NULL;
    }

    if (current_allocator->parent_allocator != NULL && !t->is_actual)
    {
        trace("memory_allocator_new:: using parent allocator");
        t = current_allocator->parent_allocator->entry;
    }

    memory_allocator_data *data = t->tail;
    if (data->offset + size > data->data_size)
    {
        trace("memory_allocator_new:: data entry full");
        if (data->next_entry == NULL)
        {
            memory_allocator_data *new_data = memory_allocator_create_data(data->data_size);
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
