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
#include <string.h>

#include "ltr_xsl.h"

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
    if (ret == NULL)
    {
        error("xml_new_node:: malloc");
        return NULL;
    }

    ret->type = type;
    ret->name = name;
    ret->uid = ret;
    return ret;
}

XMLNODE *xml_append_child(TRANSFORM_CONTEXT *pctx, XMLNODE *node, NODETYPE type)
{
  XMLNODE *ret = xml_new_node(pctx, NULL, type);
  xml_add_child(pctx, node, ret);
  return ret;
}

void xml_add_child(TRANSFORM_CONTEXT *pctx, XMLNODE *node, XMLNODE *child)
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
