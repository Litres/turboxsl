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
#include "strings.h"

#include <stdio.h>
#include <string.h>

#include "ltr_xsl.h"

int xml_strcmp(const char *l, const char *r)
{
  if(l==r) return 0;
  if(l==NULL) return -1;
  if(r==NULL) return 1;
  return strcmp(l,r);
}

int xml_strcasecmp(char *l, char *r)
{
  if(l==r) return 0;
  if(l==NULL) return -1;
  if(r==NULL) return 1;
  return strcasecmp(l,r);
}

char *xml_strdup(const char *s)
{
  if(s==NULL) return NULL;
  return xml_new_string(s, strlen(s));
}

char *xml_new_string(const char *s, size_t length)
{
  if(s==NULL) return NULL;

  char *result = memory_allocator_new(length + 1);
  memcpy(result, s, length);
  return result;
}

XMLSTRING xml_process_string(TRANSFORM_CONTEXT *pctx, XMLNODE *locals, XMLNODE *context, XMLSTRING string)
{
  if(!string) return NULL;

  if(!strchr(string->s,'{') && !strchr(string->s,'}')) {
    return xmls_new_string_literal(string->s);
  }

  XMLSTRING res = xmls_new(100);
  char *s = xml_strdup(string->s);
  while(*s) {
    if(*s == '{') { //evaluate
      ++s;
      if(*s=='{') {
        xmls_add_char(res, *s++);
        continue;
      }
      char *p=strchr(s,'}');
      if(p)
        *p=0;
      char *r = xpath_eval_string(pctx, locals, context, xpath_find_expr(pctx,xmls_new_string_literal(s)));
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

  return res;
}

/***************************** XMLSTRINGS ****************************/
XMLSTRING xmls_new(size_t bsize)
{
  XMLSTRING ret;
  ret = memory_allocator_new(sizeof(struct _xmls));
  ret->allocated = bsize;
  ret->len = 0;
  ret->s = memory_allocator_new(bsize + 1);
  return ret;
}

XMLSTRING xmls_new_string(const char *s, size_t length)
{
  if(s==NULL) return NULL;

  XMLSTRING result = xmls_new(length);
  memcpy(result->s, s, length);
  result->len = length;

  return result;
}

XMLSTRING xmls_new_string_literal(const char *s)
{
  if(s==NULL) return NULL;

  return xmls_new_string(s, strlen(s));
}

void xmls_add_char(XMLSTRING s, char c)
{
  if(s->len >= s->allocated-2) {
    char *data = s->s;
    size_t data_size = s->allocated;
    s->allocated = s->allocated*2 + 1;
    s->s = memory_allocator_new(s->allocated);
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

void xmls_append(XMLSTRING s, XMLSTRING value)
{
  if (value == NULL) return;
  xmls_add_str(s, value->s);
}

void xmls_add_str(XMLSTRING s, const char *d)
{
  unsigned l;

  if(!d)
    return;
  l = strlen(d)+1;
  if(l==0)
    return;

  if(s->len+l >= s->allocated) {
    char *data = s->s;
    size_t data_size = s->allocated;
    s->allocated = s->allocated*2 + l;
    s->s = memory_allocator_new(s->allocated);
    memcpy(s->s, data, data_size);
  }
  memcpy(s->s+s->len,d,l); // end null included!
  s->len = s->len+l-1;
}

char *xmls_detach(XMLSTRING s)
{
  if(!s) return NULL;
  return s->s;
}

int xmls_equals(XMLSTRING a, XMLSTRING b)
{
  if(a == NULL && b == NULL) return 1;

  if(a == NULL || b == NULL) return 0;
  if(a->len != b->len) return 0;

  return bcmp(a->s, b->s, a->len) == 0;
}

short *utf2ws(char *s)
{
short *ws;
short u;
int i,j;

  if(!s)
    return NULL;
  ws = memory_allocator_new((strlen(s) + 1) * sizeof(short));
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