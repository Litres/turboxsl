/*
 *  TurboXSL XML+XSLT processing library
 *  XML parser
 *
 *
 *  (c) Egor Voznessenski, voznyak@mail.ru
 *
 *  $Id$
 *
**/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>

#include "ltr_xsl.h"

enum {INIT, OPEN, CLOSE, COMMENT, BODY, TEXT, XMLDECL, INSIDE_TAG, ATTR_VALUE, CDATA, DOCTYPE, ERROR} state;


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
  char * buf = memory_allocator_new(s - p + 2);
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
      error("decode_entity:: invalid numeric entity");
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
      error("decode_entity:: unknown entity &%c%c%c\n",s[0][0],s[0][1],s[0][2]);
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
        } else if(!memcmp(p,"!DOCTYPE",8)) {
          p+=8;
          state = DOCTYPE;
        } else if(*p=='!') {
          error("do_parse:: unknown instruction");
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
          error("do_parse:: closing tag mismatch <%s> </%s>",ret->name,p);
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
      case DOCTYPE:
        while(!(p[0]==']' && p[1]=='>')) p++;
        p+=2;
        state = TEXT;
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

XMLNODE *XMLParse(XSLTGLOBALDATA *gctx, char *document)
{
  info("XMLParse:: document");
  XMLNODE *ret;
  char *fn = hash("(string)",-1,0);

  memory_allocator *cache = memory_allocator_create();
  memory_allocator_add_entry(cache, pthread_self(), 1000000);
  memory_allocator_set_current(cache);

  ret = do_parse(gctx, document, fn);
  renumber_children(ret);

  ret->allocator = cache;
  return ret;
}

//*
XMLNODE *
XMLParseFile(XSLTGLOBALDATA *gctx, char *file)
{
  info("XMLParseFile:: file %s", file);
  return xml_parse_file(gctx, file, 0);
}

XMLNODE *xml_parse_file(XSLTGLOBALDATA *gctx, char *file, int has_cache)  {
	XMLNODE   *ret;
	FILE      *pFile;
	char      *buffer;
	unsigned   length;
	long       size;

    debug("xml_parse_file:: file %s", file);
	if (file == NULL)
		return NULL;

	file = hash(file,-1,0);
	if ((pFile = fopen(file, "r")) == NULL) {
		error("xml_parse_file:: can't open %s: %s", file, strerror(errno));
		return NULL;
	}

	if (fseek(pFile, 0, SEEK_END) || (size = ftell(pFile)) == EOF || fseek(pFile, 0, SEEK_SET)) {
		fclose(pFile);
		return NULL;
	}

	buffer = (char*) malloc(size + 10);
	if (buffer == NULL) {
		fclose(pFile);
		return NULL;
	}

	length = fread(buffer, 1, size, pFile);
	fclose(pFile);

	if (length < 0) {
		free(buffer);
		return NULL;
	}
	buffer[length] = 0;

    memory_allocator *cache = NULL;
    if (has_cache == 0)
    {
        cache = memory_allocator_create();
      memory_allocator_add_entry(cache, pthread_self(), 1000000);
      memory_allocator_set_current(cache);
    }

	ret = do_parse(gctx, buffer, file);
    free(buffer);

    if (ret == NULL) {
      memory_allocator_release(cache);
      return NULL;
    }
    renumber_children(ret);
    ret->allocator = cache;

	return ret;
}
//*/
