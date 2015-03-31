/*
 *  TurboXSL XML+XSLT processing library
 *  Dictionary for hashed names -> pointers conversion
 *
 *
 *  (c) Egor Voznessenski, voznyak@mail.ru
 *
 *  $Id$
 *
**/

#include "xmldict.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "logger.h"

typedef struct _dict_entry {
  const char *name;
  const void *data;
  unsigned next;
} XMLDICTENTRY;

struct _dict {
  XMLDICTENTRY *entries;
  unsigned allocated;
  unsigned used;
  unsigned hash[128];
};

/*
 * sdbm hash function
 */
unsigned long hash_function(unsigned char *str)
{
  unsigned long hash = 0;
  int c;

  while ((c = *str++))
    hash = c + (hash << 6) + (hash << 16) - hash;

  return hash;
}

unsigned int bucket_number(char *str)
{
  return hash_function(str) % 127;
}

XMLDICT *dict_new(unsigned size)
{
  XMLDICT *dict;
  dict = malloc(sizeof(XMLDICT));
  if(!size) size = 100;

  if(dict) {
    memset(dict,0,sizeof(XMLDICT));
    dict->allocated = size;
    dict->entries = malloc(size * sizeof(XMLDICTENTRY));
    if(!dict->entries) {
      free(dict);
      dict = 0;
    }
  }
  if(!dict) {
    error("dict_new:: failed to allocate storage");
  }
  return dict;
}

void dict_free(XMLDICT *dict)
{
  if(dict) {
    if(dict->entries)
      free(dict->entries);
    free(dict);
  }
}

const void *dict_find(XMLDICT *dict, const char *name)
{
  if(!dict || !name)
    return NULL;

  unsigned h = bucket_number(name);
  for(h=dict->hash[h];h;) {
    --h;
    if(strcmp(dict->entries[h].name, name) == 0)
      return dict->entries[h].data;
    h = dict->entries[h].next;
  }
  return NULL;
}

int dict_add(XMLDICT *dict, const char *name, const void *data)
{
  unsigned h,d;

  if(!dict || !name) return 0;
  
  if(dict->used >= dict->allocated) {
    dict->allocated += 100;
    dict->entries = realloc(dict->entries,dict->allocated * sizeof(XMLDICTENTRY));
  }

  d = h = bucket_number(name);
  for(h=dict->hash[h];h;) {
    --h;
    if(strcmp(dict->entries[h].name, name) == 0)
      return 0; // already have this
    h = dict->entries[h].next;
  }
  dict->entries[dict->used].name = name;
  dict->entries[dict->used].data = data;
  dict->entries[dict->used].next = dict->hash[d];
  dict->hash[d] = ++dict->used;
  return 1;
}

void dict_replace(XMLDICT *dict, const char *name, const void *data)
{
  if(!dict || !name)
    return;

  if(dict->used >= dict->allocated) {
    dict->allocated += 100;
    dict->entries = realloc(dict->entries,dict->allocated * sizeof(XMLDICTENTRY));
  }
  dict->entries[dict->used].name = name;
  dict->entries[dict->used].data = data;

  unsigned h = bucket_number(name);
  dict->entries[dict->used].next = dict->hash[h];
  dict->hash[h] = ++dict->used;
}
