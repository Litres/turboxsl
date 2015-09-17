/*
 *  TurboXSL XML+XSLT processing library
 *  XMLNODE operations
 *
 *
 *  (c) Egor Voznessenski, voznyak@mail.ru
 *
 *  $Id$
 *
**/

#include <stdio.h>

#include "ltr_xsl.h"
#include "xsl_elements.h"

void xml_unlink_node(XMLNODE *node)
{
  if(node->prev) {
    node->prev->next = node->next;
  } else {
    if(node->parent)
      node->parent->children = node->next;
  }
  if(node->next) {
    node->next->prev = node->prev;
  }
}

/*********** for static-allocated nodes *******************/

XMLNODE *xml_new_node(TRANSFORM_CONTEXT *pctx, XMLSTRING name, NODETYPE type)
{
    XMLNODE *ret = memory_allocator_new(sizeof(XMLNODE));
    ret->type = type;
    ret->name = name;
    ret->uid = ret;
    return ret;
}

XMLNODE *xml_append_child(TRANSFORM_CONTEXT *pctx, XMLNODE *node, NODETYPE type)
{
  XMLNODE *ret = xml_new_node(pctx, NULL, type);
  xml_add_child(node, ret);
  return ret;
}

void xml_add_child(XMLNODE *node, XMLNODE *child)
{
    if (!child)
    {
        error("xml_add_child:: child is NULL");
        return;
    }

    if (node->children)
    {
        XMLNODE *prev;
        for (prev = node->children; prev->next; prev = prev->next);
        child->prev = prev;
        prev->next = child;
    }
    else
    {
        node->children = child;
    }
    child->parent = node;
}

XMLNODE *xpath_nodeset_copy(TRANSFORM_CONTEXT *pctx, XMLNODE *src)
{
  unsigned i = 0;
  XMLNODE *dst,*ret;
  if(!src)
    return NULL;  //empty nodeset

  ret = dst = xml_new_node(pctx, NULL,src->type);
  for(;;) {
    dst->original = src->original;
    dst->name = src->name;
    dst->position = ++i;
    dst->order = src->order;
    dst->children = src->children;
    dst->attributes = src->attributes;
    dst->content = src->content;
    dst->parent = src->parent;
    if(src->next) {
      src = src->next;
      dst->next = xml_new_node(pctx, NULL,src->type);
      dst->next->prev = dst;
      dst=dst->next;
    } else {
      break;
    }
  }
  return ret;
}

int is_node_parallel(XMLNODE *node)
{
    if (node->type == TEXT_NODE) return 0;

    XMLNODE *t = node->next;
    while (t != NULL)
    {
        if (t->type == ELEMENT_NODE) return 1;
        t = t->next;
    }

    return 0;
}

XMLNODE *XMLCreateDocument()
{
  memory_allocator *allocator = memory_allocator_create();
  memory_allocator_add_entry(allocator, pthread_self(), 100000);
  memory_allocator_set_current(allocator);

  XMLNODE *result = xml_new_node(NULL, xsl_s_root, EMPTY_NODE);
  result->allocator = allocator;

  return result;
}

XMLNODE *XMLCreateElement(XMLNODE *parent, char *name)
{
  XMLNODE *result = xml_new_node(NULL, xmls_new_string_literal(name), ELEMENT_NODE);
  if (parent != NULL) xml_add_child(parent, result);
  return result;
}

void XMLAddText(XMLNODE *element, char *text)
{
  XMLNODE *result = xml_new_node(NULL, NULL, TEXT_NODE);
  result->content = xmls_new_string_literal(text);
  result->parent = element;
  xml_add_child(element, result);
}

void XMLAddAttribute(XMLNODE *element, char *name, char *value)
{
  XMLNODE *attribute = xml_new_node(NULL, xmls_new_string_literal(name), ATTRIBUTE_NODE);
  attribute->content = xmls_new_string_literal(value);
  attribute->parent = element;

  attribute->next = element->attributes;
  element->attributes = attribute;
}

void XMLAddChildFromString(XSLTGLOBALDATA *context, XMLNODE *element, char *value)
{
  XMLNODE *root = xml_parse_string(context, value, 1);
  if (root == NULL)
  {
    error("XMLAddChildFromString:: fail to parse string: %s", value);
    return;
  }
  xml_add_child(element, root->children);
}
