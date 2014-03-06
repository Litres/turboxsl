/*
 *  TurboXSL XML+XSLT processing library
 *  XML parser
 *
 *
 *
 *
 *  $Id: parse.c 34247 2014-03-05 14:48:37Z evozn $
 *
**/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "ltr_xsl.h"

enum {INIT, OPEN, CLOSE, COMMENT, BODY, TEXT, XMLDECL, INSIDE_TAG, ATTR_VALUE, CDATA, ERROR} state;


static
char *skip_spaces(char *p, unsigned *ln)  // TODO add UTF8 spaces
{
  for(;;++p) {
    if(*p=='\n')
       ++(*ln);
    if(x_is_ws(*p))
      continue;
    return p;
  }
}

static
char *make_string(char *p, char *s)
{
  char * buf = malloc(s-p+2);
  memcpy(buf,p,s-p);
  buf[s-p]=0;
  return buf;
}

static
void decode_entity(char **s, XMLSTRING d)
{
  unsigned u;
  if(**s == '#') {
    ++(*s);
    if(**s=='x'||**s=='X') {
      ++(*s);
      u=strtoul(*s,s,16);
    } else {
      u=strtoul(*s,s,10);
    }
    if(**s==';')
      ++(*s);
    else {
      fprintf(stderr,"Invalid numeric entity\n");
      u = '?';
    }
  } else {
    if(match(s,"amp;"))
      u = '&';
    else if(match(s,"quot;"))
      u = '"';
    else if(match(s,"lt;"))
      u = '<';
    else if(match(s,"gt;"))
      u = '>';
    else if(match(s,"apos;"))
      u = '\'';
    else {
      fprintf(stderr,"Unknown entity &%c%c%c\n",s[0][0],s[0][1],s[0][2]);
      u = '&';
    }
  }
  xmls_add_utf(d,u);
}

static
char *make_unescaped_string(char *p, char *s)
{
  char c;
  XMLSTRING buf = xmls_new(s-p);

  while(p<s) {
    c = *p++;
    if(c=='&') {
      decode_entity(&p, buf);
    } else {
      xmls_add_char(buf, c);
    }
  }
  return xmls_detach(buf);
}


static
int can_name(char c)
{
  if(c>='a' && c<='z')
     return 1;
  if(c=='-' || c=='_' || c==':')
     return 1;
  if(c>='0' && c<='9')
     return 1;
  if(c>='A' && c<='Z')
     return 1;
  return 0;
}

XMLNODE *do_parse(XSLTGLOBALDATA *gctx, char *document, char *uri)
{
  XMLNODE *ret;
  XMLNODE *current = NULL;
  XMLNODE *attr;
  XMLNODE *previous = NULL;
  char *p = document;
  char *c;
  char *buffer;
  unsigned comment_depth = 0;
  unsigned ln = 0;
  init_hash();

  state = INIT;
  ret = xml_new_node(NULL,"root",EMPTY_NODE);
  ret->file = uri;

  while(*p) {
    if(state==ERROR)
      break;

    switch(state) {
      case INIT:  // skip all until opening element
        if(*p == '\n')
          ++ln;
        if(*p++ == '<')
          state = OPEN;
        break;
      case XMLDECL:
        while(p[0]!='?' || p[1]!='>') {
          p++;
        }
        p+=2;
        state = INIT;
        break;
      case COMMENT:
        if(*p=='\n')
          ++ln;
        if(p[0]=='-' && p[1]=='-' && p[2]=='>') {
          p += 3;
          if(--comment_depth == 0)
            state = TEXT;
        } else {
          ++p;
        }
        break;
      case OPEN:
        if(*p == '?') {
          ++p;
          state = XMLDECL; // skip until ?>
        }
        else if(p[0]=='!' && p[1]=='-' && p[2]=='-') { // comment
          p += 3;
          comment_depth++;
          state = COMMENT;
        } else if(!memcmp(p,"![CDATA[",8)) {// cdata
          p+=8;
          state = CDATA;
        } else if(*p=='!') {
          fprintf(stderr,"DOCTYPE instructions not supported!\n");
          return NULL;
        } else if(*p=='/') {// closing </element>
          state = CLOSE;
          ++p;
        } else {// start of element
          p = skip_spaces(p,&ln);
          for(c=p; can_name(*c);++c)
            ;
          current = xml_new_node(NULL,hash(p,c-p,0), ELEMENT_NODE);
          current->prev = previous;
          current->file = uri;
          current->line = ln;
          if(previous)
            previous->next = current;
          else  // no previous sibling, add as first child to parent node
            ret->children = current;
          current->parent = ret;
          current->next = NULL;
          if(!ret)
            ret = current; // XXX first node treated as root!
          p = skip_spaces(c,&ln);
          state = INSIDE_TAG;
        }
        break;
      case CLOSE:
        p = skip_spaces(p,&ln);
        for(c=p; can_name(*c);++c)
            ;
        if(!memcmp(ret->name,p,c-p)) {
            for(p=c;*p!='>';++p)
              ;
            ++p;
            state = TEXT;
            previous = ret;
            ret = previous->parent;
        } else {
          *c = 0;
          fprintf(stderr,"closing tag mismatch <%s> </%s>\n",ret->name,p);
          state = ERROR;
        }
        break;
      case INSIDE_TAG:
        if(*p=='\n')
          ++ln;
        if(*p=='>') { // tag ended, possible children follow
          ++p;
          ret = current;
          current = previous = NULL;
          state = TEXT;
        } else if(p[0]=='/' && p[1]=='>') { // self closed tag, make it previous and proceed to next;
          previous = current;
          p+=2;
          state = TEXT;
        } else { // must be an attribute of the tag
          p = skip_spaces(p,&ln);
          for(c=p; can_name(*c);++c)
            ;
          attr = xml_new_node(NULL,hash(p,c-p,0), ATTRIBUTE_NODE);
          attr->file = uri;
          attr->line = ln;
          attr->next = current->attributes;
          current->attributes = attr;
          attr->parent = current;
          p = skip_spaces(c,&ln);
          if(*p == '=') {
            p=skip_spaces(p+1,&ln);
            state = ATTR_VALUE;
          }
        }
        break;
      case CDATA:
        for(c=p;*c;++c) {
          if(*c=='\n')
            ++ln;
          if(c[0]==']' && c[1]==']' && c[2]=='>') {
            break;
          }
        }
        if(c>p)
        {
          current = xml_new_node(NULL,NULL, TEXT_NODE);
          current->file = uri;
          current->line = ln;
          current->content = (void *)make_string(p,c);
          current->flags = XML_FLAG_NOESCAPE;
          current->prev = previous;
          if(previous)
            previous->next = current;
          else  // no previous sibling, add as first child to parent node
            ret->children = current;
          current->parent = ret;
          current->next = NULL;
          previous = current;
          p = c + 3;
        }
        state = INIT;
        break;
      case TEXT:
        for(c=p;(*c && *c != '<');++c)
          if(*c=='\n')
          ++ln;
        if(c>p)
        {
          current = xml_new_node(NULL, NULL, TEXT_NODE);
          current->file = uri;
          current->line = ln;
          current->content = (void *)make_unescaped_string(p,c);
          current->prev = previous;
          if(previous)
            previous->next = current;
          else  // no previous sibling, add as first child to parent node
            ret->children = current;
          current->parent = ret;
          current->next = NULL;
          previous = current;
          p = c;
        }
        state = INIT;
        break;
      case ATTR_VALUE:
        if(!(*p == '"' || *p=='\'' || *p=='`')) {
          state = ERROR;
        } else {
          char endchar = *p++;
          for(c=p;*c != endchar;++c)
            ;
          attr->content = (void *)make_unescaped_string(p,c);
        }
        p = skip_spaces(c+1,&ln);
        state = INSIDE_TAG;
        break;
    }
  }
  return state==ERROR?NULL:ret;
}

static
void renumber_children(XMLNODE *node)
{
  unsigned pos = 1;
  for(;node;node=node->next) {
    if(node->type==TEXT_NODE)
      continue;
    node->position = pos++;
    if(node->children)
      renumber_children(node->children);
  }
}

#define CHUNK (10000)

XMLNODE *XMLParse(XSLTGLOBALDATA *gctx, char *document)
{
  XMLNODE *ret;
  char *fn = hash("(string)",-1,0);

  ret = do_parse(gctx, document, fn);
  renumber_children(ret);
  return ret;
}


XMLNODE *XMLParseFile(XSLTGLOBALDATA *gctx, char *file)
{
XMLNODE *ret;
FILE *f;
char *buffer;
char *p;
unsigned length,nread;

  file = hash(file,-1,0);
  f = fopen(file, "r");
  if(f) {
    buffer = (char *)malloc(length=CHUNK);
    p = buffer;
    while((nread = fread(p,1,CHUNK,f)) == CHUNK)
    {
      length += CHUNK;
      buffer = realloc(buffer, length);
      p = buffer+length-CHUNK;
    }
    p[nread] = 0;
    fclose(f);
  } else {
    fprintf(stderr, "failed to open %s\n",file);
    return NULL;
  }

  ret = do_parse(gctx, buffer, file);
  renumber_children(ret);
  free(buffer);

  return ret;
}
