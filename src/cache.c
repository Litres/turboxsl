/*
 *  TurboXSL XML+XSLT processing library
 *  Hash for string values
 *
 *
 *
 *
 *  $Id: cache.c 34151 2014-03-03 18:41:50Z evozn $
 *
**/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <pthread.h>

#include "ltr_xsl.h"

#define NSEEDS (0x400)



typedef struct _hashelem {
  char *content;
  struct _hashelem *next;
  int len;
} hashelem;

hashelem *seeds[NSEEDS];
pthread_mutex_t hashMutex;
static int initialized = 0;

void init_hash()
{
  if(!initialized) {
    memset(seeds,0,sizeof(hashelem*)*NSEEDS);
    pthread_mutex_init(&hashMutex, NULL);
    initialized = 1;
  }
}

void drop_hash()
{
  unsigned i;
  hashelem *sptr, *n;

  for(i=0;i<NSEEDS;++i) {
    for(sptr=seeds[i];sptr;sptr=n) {
      n = sptr->next;
      free(sptr->content);
      free(sptr);
    }
  }
  initialized = 0;
}

static unsigned hashfunction(char *name, int len) // TODO: improve randomization
{
  unsigned seed;
  unsigned p;

  seed = (name[0]<<3)^len;
  if(len>4) {
    for(p=len-2;p;p>>=1) {
      seed = ((seed<<1) ^ name[p]);
    }
  }
  return (seed%NSEEDS);
}

static hashelem *hash_new(char *name, int len)
{
  hashelem *ret = (hashelem *)malloc(sizeof(hashelem));
  char *buf = (char *)malloc(len+1);
  memcpy(buf,name,len);
  buf[len] = 0;
  ret->next = 0;
  ret->content = buf;
  ret->len = len;
  return ret;
}

char *find_hash(char *name, int len, int lock)
{
  hashelem *hep;
  unsigned seed;

  if(name==NULL)
    return NULL;
  if(lock)
    pthread_mutex_lock(&hashMutex);
  if(len<0)
    len = strlen(name);
  seed = hashfunction(name,len);
  hep = seeds[seed];
  if(hep) {
    while(hep) {
      if(hep->len == len && name==hep->content) {
        if(lock)
          pthread_mutex_unlock(&hashMutex);
        return hep->content;
      }
      if(hep->len == len && (0==memcmp(name,hep->content,len))) {
        if(lock)
          pthread_mutex_unlock(&hashMutex);
        return hep->content;
      }
      hep = hep->next;
    }
  }
  if(lock)
    pthread_mutex_unlock(&hashMutex);
  return NULL;
}

char *add_hash(char *name, int len, int lock)
{
  hashelem *new;
  hashelem *hep;
  unsigned seed;

  if(name==NULL)
    return NULL;
  if(lock)
    pthread_mutex_lock(&hashMutex);
  if(len<0)
    len = strlen(name);
  seed = hashfunction(name,len);
  hep = seeds[seed];
  new = hash_new(name,len);
  new->next = hep;
  seeds[seed] = new;
  if(lock)
    pthread_mutex_unlock(&hashMutex);
  return new->content;
}

char *hash(char *name, int len, int lock)
{
char *ret;

  if(!name)
    return NULL;
  if(len < 0)
    len = strlen(name);
  if(lock)
    pthread_mutex_lock(&hashMutex);
  if((ret=find_hash(name, len, 0))==NULL)
    ret = add_hash(name, len, 0);
  if(lock)
    pthread_mutex_unlock(&hashMutex);
 return ret;
}
