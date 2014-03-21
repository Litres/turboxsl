/*
 *  TurboXSL XML+XSLT processing library
 *  Minor support routines
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
#include <unistd.h>

#include "ltr_xsl.h"

int x_can_number(char *p)
{
  if(*p=='-')
    ++p;
  if(*p=='.')
    ++p;
  return (*p >= '0' && *p <= '9')?1:0;
}

int x_is_ws(char c)
{
  if(c==' '||c=='\t'||c=='\n'||c=='\r')
    return 1;
  return 0;
}

int x_is_namechar(char c)
{
  if((c>='A'&&c<='Z')||(c>='a'&&c<='z')||(c>='0'&&c<='9')||c=='_'||c=='-'||c==':'||c=='.')
    return 1;
  return 0;
}

int x_is_selchar(char c)
{
  if(x_is_namechar(c)||c=='/'||c=='.'||c=='*')
    return 1;
  return 0;
}

char *xml_get_attr(XMLNODE *node, char *name)
{
  XMLNODE *attr;
  while(node->type==EMPTY_NODE)
    node=node->children;

  for(attr=node->attributes;attr;attr=attr->next)
    if(attr->name == name)
      return attr->content;
  name = hash(name,-1,1);
  for(attr=node->attributes;attr;attr=attr->next)
    if(attr->name == name)
      return attr->content;
  return NULL;
}

