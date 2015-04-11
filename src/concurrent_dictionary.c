#include "concurrent_dictionary.h"

#include <stdlib.h>
#include <string.h>

#include <ck_ht.h>

#include "logger.h"
#include "allocator.h"

struct concurrent_dictionary_ {
    ck_ht_t table;
};

static void *ht_malloc(size_t r)
{
    return memory_allocator_new(r);
}

static void ht_free(void *p, size_t b, bool r)
{
    return;
}

static struct ck_malloc my_allocator = {
    .malloc = ht_malloc,
    .free = ht_free
};

concurrent_dictionary *concurrent_dictionary_create()
{
    concurrent_dictionary *dictionary = malloc(sizeof(concurrent_dictionary));
    if (dictionary == NULL)
    {
        error("concurrent_dictionary_create:: create");
        return NULL;
    }
    memset(dictionary, 0, sizeof(concurrent_dictionary));

    unsigned int mode = CK_HT_MODE_BYTESTRING;
    if (ck_ht_init(&(dictionary->table), mode, NULL, &my_allocator, 2, 6602834) == false)
    {
        error("concurrent_dictionary_create:: hash table");
        return NULL;
    }

    return dictionary;
}

void concurrent_dictionary_release(concurrent_dictionary *dictionary)
{
    ck_ht_destroy(&(dictionary->table));
    free(dictionary);
}

void *concurrent_dictionary_find(concurrent_dictionary *dictionary, const char *key)
{
    ck_ht_hash_t h;
    size_t key_length = strlen(key);
    ck_ht_hash(&h, &(dictionary->table), key, key_length);

    ck_ht_entry_t entry;
    ck_ht_entry_key_set(&entry, key, key_length);

    if (!ck_ht_get_spmc(&(dictionary->table), h, &entry)) return NULL;

    return ck_ht_entry_value(&entry);
}

int concurrent_dictionary_add(concurrent_dictionary *dictionary, const char *key, void *value)
{
    size_t key_length = strlen(key);
    ck_ht_hash_t h;
    ck_ht_hash(&h, &(dictionary->table), key, key_length);

    ck_ht_entry_t entry;
    ck_ht_entry_set(&entry, h, key, key_length, value);

    return ck_ht_put_spmc(&(dictionary->table), h, &entry);
}
