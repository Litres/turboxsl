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
  if(c == ' ' || c == '\t' || c == '\n' || c == '\r')
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


XMLSTRING xml_get_attr(XMLNODE *node, XMLSTRING name)
{
  XMLNODE *attr;
  while(node->type==EMPTY_NODE)
    node=node->children;

  for(attr=node->attributes;attr;attr=attr->next)
    if(xmls_equals(attr->name, name)) return attr->content;

  return NULL;
}

XMLNODE *XMLFindNodes(TRANSFORM_CONTEXT *pctx, XMLNODE *xml, char *expression)
{
  XMLNODE *root_node = pctx->root_node;

  XMLNODE *locals = xml_new_node(pctx,NULL,EMPTY_NODE);
  pctx->root_node = xml;
  RVALUE result;
  xpath_eval_expression(pctx, locals, xml, xmls_new_string_literal(expression), &result);

  pctx->root_node = root_node;

  if (result.type == VAL_NODESET) return result.v.nodeset;
  if (result.type == VAL_STRING)
  {
    XMLNODE *node = xml_new_node(pctx, NULL, TEXT_NODE);
    node->content = xmls_new_string_literal(result.v.string);
    return node;
  }

  return NULL;
}

char *XMLStringValue(XMLNODE *xml)
{
  return node2string(xml);
}

char **XMLAttributes(XMLNODE *xml)
{
  unsigned int count = 0;
  XMLNODE *attribute = xml->attributes;
  while (attribute)
  {
    count++;
    attribute = attribute->next;
  }
  if (count == 0) return NULL;

  // last is terminator
  char **result = memory_allocator_new((2 * count + 1) * sizeof(char *));
  unsigned int p = 0;
  attribute = xml->attributes;
  while (attribute)
  {
    result[p++] = attribute->name->s;
    result[p++] = attribute->content->s;
    attribute = attribute->next;
  }

  return result;
}
