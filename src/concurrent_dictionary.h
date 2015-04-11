#ifndef CONCURRENT_DICTIONARY_H_
#define CONCURRENT_DICTIONARY_H_

typedef struct concurrent_dictionary_ concurrent_dictionary;

concurrent_dictionary *concurrent_dictionary_create();

void concurrent_dictionary_release(concurrent_dictionary *dictionary);

void *concurrent_dictionary_find(concurrent_dictionary *dictionary, const char *key);

int concurrent_dictionary_add(concurrent_dictionary *dictionary, const char *key, void *value);

#endif
