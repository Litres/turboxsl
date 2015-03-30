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


#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "xmldict.h"
#include "ltr_xsl.h"

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

void *dict_find(XMLDICT *dict, char *name)
{
  unsigned h;

  if(!dict || !name)
    return NULL;

  h = (unsigned)(long)name;
  h = 127 & (h>>5 ^ h>>11 ^ h);
  for(h=dict->hash[h];h;) {
    --h;
    if(dict->entries[h].name == name)
      return dict->entries[h].data;
    h = dict->entries[h].next;
  }
  return NULL;
}

int dict_add(XMLDICT *dict, char *name, void *data)
{
  unsigned h,d;

  if(!dict || !name) return 0;
  
  if(dict->used >= dict->allocated) {
    dict->allocated += 100;
    dict->entries = realloc(dict->entries,dict->allocated * sizeof(XMLDICTENTRY));
  }
  h = (unsigned)(long)name;
  d = h = 127 & (h>>5 ^ h>>11 ^ h);
  for(h=dict->hash[h];h;) {
    --h;
    if(dict->entries[h].name == name)
      return 0; // already have this
    h = dict->entries[h].next;
  }
  dict->entries[dict->used].name = name;
  dict->entries[dict->used].data = data;
  dict->entries[dict->used].next = dict->hash[d];
  dict->hash[d] = ++dict->used;
  return 1;
}

void dict_replace(XMLDICT *dict, char *name, void *data)
{
  unsigned h;

  if(!dict || !name)
    return;
  if(dict->used >= dict->allocated) {
    dict->allocated += 100;
    dict->entries = realloc(dict->entries,dict->allocated * sizeof(XMLDICTENTRY));
  }
  dict->entries[dict->used].name = name;
  dict->entries[dict->used].data = data;
  h = (unsigned)(long)name;
  h = 127 & (h>>5 ^ h>>11 ^ h);
  dict->entries[dict->used].next = dict->hash[h];
  dict->hash[h] = ++dict->used;
}
