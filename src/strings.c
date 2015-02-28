/*
 *  TurboXSL XML+XSLT processing library
 *  common string functions
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

#include "ltr_xsl.h"

int xml_strcmp(char *l, char *r)
{
    if(l==r)
	return 0;   //fast path for hashed strings and case of both NULLs
    if(l==NULL)
	return -1;
    if(r==NULL)
	return 1;
    return strcmp(l,r);
}

int xml_strcasecmp(char *l, char *r)
{
    if(l==r)
	return 0;   //fast path for hashed strings and case of both NULLs
    if(l==NULL)
	return -1;
    if(r==NULL)
	return 1;
    return strcasecmp(l,r);
}

char *xml_strdup(char *s)
{
  if(s==NULL)
    return NULL;

  size_t length = strlen(s);
  char *result = memory_cache_allocate(length + 1);
  memcpy(result, s, length);
  return result;
}

char *xml_process_string(TRANSFORM_CONTEXT *pctx, XMLNODE *locals, XMLNODE *context, char *s)
{
  char *s_str, *p, *r;
  XMLSTRING res;

  if(!s)
    return NULL;

  if(!strchr(s,'{') && !strchr(s,'}')) {
    return xml_strdup(s);
  }

  res = xmls_new(100);

  while(*s) {
    if(*s == '{') { //evaluate
      ++s;
      if(*s=='{') {
        xmls_add_char(res, *s++);
        continue;
      }
      p=strchr(s,'}');
      if(p)
        *p=0;
      r = xpath_eval_string(pctx, locals, context, xpath_find_expr(pctx,s));
      if(r) {
        xmls_add_str(res, r);
      }
      if(p) {
        *p = '}';
        s = p+1;
      }
      else break;
    } else {
      if(*s=='}' && s[1]=='}')
        ++s;
      xmls_add_char(res, *s++);
    }
  }

  return xmls_detach(res);
}

int x_is_empty(char *s)
{
  if(s) {
    while(*s)
      if(!x_is_ws(*s++))
        return 0;
  }
  return 1;
}

char *xml_unescape(char *s)
{
  XMLSTRING d;
  if(s==NULL)
    return NULL;
  d = xmls_new(strlen(s));
  while(*s) {
    if(*s=='&') {
      if(0==memcmp(s,"&amp;",5)) {
        s += 5;
        xmls_add_char(d,'&');
        continue;
      } else if(0==memcmp(s,"&quot;",6)) {
        s += 6;
        xmls_add_char(d,'"');
        continue;
      } else if(0==memcmp(s,"&lt;",4)) {
        s += 4;
        xmls_add_char(d,'<');
        continue;
      } else if(0==memcmp(s,"&gt;",4)) {
        s += 4;
        xmls_add_char(d,'>');
        continue;
      }
    }
    xmls_add_char(d,*s++);
  }
  return xmls_detach(d);
}

/***************************** XMLSTRINGS ****************************/
XMLSTRING xmls_new(unsigned bsize)
{
  XMLSTRING ret;
  ret = memory_cache_allocate(sizeof(struct _xmls));
  ret->allocated = bsize;
  ret->len = 0;
  ret->s = memory_cache_allocate(bsize+1);
  ret->s[0] = 0;
  return ret;
}

void xmls_add_char(XMLSTRING s, char c)
{
  if(s->len >= s->allocated-2) {
    char *data = s->s;
    unsigned int data_size = s->allocated;
    s->allocated = s->allocated*2 + 1;
    s->s = memory_cache_allocate(s->allocated);
    memcpy(s->s, data, data_size);
  }
  s->s[s->len++] = c;
  s->s[s->len] = 0;
}

void xmls_add_utf(XMLSTRING s, unsigned u)
{
  if(u<128)
    xmls_add_char(s,u);
  else if(u<0x800) {
    xmls_add_char(s,0xC0|(u>>6));
    xmls_add_char(s,0x80|(u&0x3F));
  }
  else if(u<0x10000) {
    xmls_add_char(s,0xE0|(u>>12));
    xmls_add_char(s,0x80|((u>>6)&0x3F));
    xmls_add_char(s,0x80|(u&0x3F));
  }
  else if(u<0x200000) {
    xmls_add_char(s,0xF0|(u>>18));
    xmls_add_char(s,0x80|((u>>12)&0x3F));
    xmls_add_char(s,0x80|((u>>6)&0x3F));
    xmls_add_char(s,0x80|(u&0x3F));
  }
}

void xmls_add_str(XMLSTRING s, char *d)
{
  unsigned l;

  if(!d)
    return;
  l = strlen(d)+1;
  if(l==0)
    return;

  if(s->len+l >= s->allocated) {
    char *data = s->s;
    unsigned int data_size = s->allocated;
    s->allocated = s->allocated*2 + l;
    s->s = memory_cache_allocate(s->allocated);
    memcpy(s->s, data, data_size);
  }
  memcpy(s->s+s->len,d,l); // end null included!
  s->len = s->len+l-1;
}

char *xmls_detach(XMLSTRING s)
{
  char *ret;
  if(!s)
    return NULL;
  ret = s->s;
  return ret;
}


short *utf2ws(char *s)
{
short *ws;
short u;
int i,j;

  if(!s)
    return NULL;
  ws = memory_cache_allocate((strlen(s)+1)*sizeof(short));
  for(i=j=0;s[i];++i) {
    u = 0;
    if(s[i]&0x80) {
      if((s[i]&0xE0)==0xC0) {
        u = ((s[i]&0x1F)<<6)|(s[i+1]&0x3F);
        ++i;
      } else if((s[i]&0xF0)==0xE0) {
        u = ((s[i]&0x0F)<<12)|((s[i+1]&0x3F)<<6)|(s[i+2]&0x3F);
        i+=2;
      }
    } else {
      u = s[i];
    }
    ws[j++]=u;
  }
  ws[j] = 0;
  return ws;
}