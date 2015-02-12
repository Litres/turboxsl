#include "node_cache.h"

#include <stdlib.h>
#include <string.h>

#include "ltr_xsl.h"

typedef struct node_cache_entry {
    pthread_t thread;
    unsigned char *data;
    size_t data_size;
    size_t offset;
    struct node_cache_entry *next;
} node_cache_entry;

struct node_cache_ {
    struct node_cache_entry *entry;
};

node_cache *node_cache_create()
{
    node_cache *cache = malloc(sizeof(node_cache));
    if (cache == NULL)
    {
        error("node_cache_create:: malloc");
        return NULL;
    }

    memset(cache, 0, sizeof(node_cache));
    return cache;
}

void node_cache_release(node_cache *cache)
{
    node_cache_entry *entry = cache->entry;
    while (entry != NULL)
    {
        node_cache_entry *t = entry->next;
        free(entry);
        entry = t;
    }
    free(cache);
}

void node_cache_add_entry(node_cache *cache, pthread_t thread, size_t size)
{
    node_cache_entry *e = malloc(sizeof(node_cache_entry));
    if (e == NULL)
    {
        error("node_cache_add_entry:: entry NULL");
        return;
    }

    memset(e, 0, sizeof(node_cache_entry));
    e->thread = thread;
    e->data_size = sizeof(XMLNODE) * size;
    e->data = malloc(e->data_size);
    if (e->data == NULL)
    {
        error("node_cache_add_entry:: data NULL");
        return;
    }

    if (cache->entry == NULL)
    {
        cache->entry = e;
    }
    else
    {
        node_cache_entry *t = cache->entry;
        while (t->next != NULL) t = t->next;
        t->next = e;
    }
}

void *node_cache_get(node_cache *cache)
{
    pthread_t self = pthread_self();
    node_cache_entry *t = cache->entry;
    while (t != NULL && t->thread != self) t = t->next;
    if (t == NULL || t->thread != self)
    {
        error("node_cache_get:: unknown thread");
        return NULL;
    }

    if (t->offset == t->data_size)
    {
        debug("node_cache_get:: no free nodes");
        return NULL;
    }

    XMLNODE *node = (XMLNODE *)(t->data + t->offset);
    t->offset = t->offset + sizeof(XMLNODE);

    return node;
}
