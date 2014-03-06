/*
 *  TurboXSL XML+XSLT processing library
 *  XMLNODE operations
 *
 *
 *
 *
 *  $Id: nodes.c 34255 2014-03-05 17:33:01Z evozn $
 *
**/

#include <stdio.h>
#include <stdlib.h>
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

static void free_attributes(XMLNODE *attributes)
{
  XMLNODE *n;
  while(attributes) {
    n = attributes->next;
    free(attributes->content);
    rval_free(&(attributes->extra));
    free(attributes);
    attributes = n;
  }
}

void xml_free_node(TRANSFORM_CONTEXT *pctx, XMLNODE *node)
{
XMLNODE *next;

  if(!node)
    return;
  xml_free_node(pctx, node->children);
  free_attributes(node->attributes);
  while(node) {
    next = node->next;
    if(node->content)
      free(node->content);
    rval_free(&(node->extra));
    nfree(pctx, node);
    node = next;
  }

}

/*********** for static-allocated nodes *******************/

void xml_cleanup_node(TRANSFORM_CONTEXT *pctx, XMLNODE *node)
{
  if(!node)
    return;
  xml_free_node(pctx, node->children);
  free_attributes(node->attributes);
  rval_free(&(node->extra));
  if(node->content) {
    free(node->content);
    node->content = NULL;
  }
}  

void xml_clear_node(TRANSFORM_CONTEXT *pctx, XMLNODE *node)
{
  memset(node,0,sizeof(XMLNODE));
}

void xml_replace_node(TRANSFORM_CONTEXT *pctx, XMLNODE *node, XMLNODE *newnode)
{
  newnode->parent = node->parent;
  if(node->prev) {
    node->prev->next = newnode;
    newnode->prev = node->prev;
  } else {
    node->parent->children = newnode;
    newnode->prev = NULL;
  }
  newnode->next = node->next;
  node->next = NULL;
  if(newnode->next)
    newnode->next->prev = newnode;
  xml_free_node(pctx,node);
}

static char nuid = 0;

XMLNODE *xml_new_node(TRANSFORM_CONTEXT *pctx, char *name, NODETYPE type)
{
XMLNODE *ret;
  if(pctx && pctx->node_cache) {
    pthread_mutex_lock(&(pctx->lock));
    ret = pctx->node_cache;
    pctx->node_cache = ret->next;
    pthread_mutex_unlock(&(pctx->lock));
  } else {
    ret = (XMLNODE *)malloc(sizeof(XMLNODE));
  }
  memset(ret,0,sizeof(XMLNODE));
  ret->type = type;
  ret->name = name;
  ret->uid = nuid++;
  return ret;
}

XMLNODE *xml_append_child(TRANSFORM_CONTEXT *pctx, XMLNODE *node, NODETYPE type)
{
XMLNODE *ret, *prev;

  ret = xml_new_node(pctx, NULL,type);
  xml_add_child(pctx, node, ret);
  return ret;
}

void nfree(TRANSFORM_CONTEXT *pctx, XMLNODE *node)
{
  if(pctx) {
    pthread_mutex_lock(&(pctx->lock));
    memset(node,0,sizeof(XMLNODE));
    node->next = pctx->node_cache;
    pctx->node_cache = node;
    pthread_mutex_unlock(&(pctx->lock));
  } else {
    free(node);
  }
}

void xml_add_child(TRANSFORM_CONTEXT *pctx, XMLNODE *node,XMLNODE *child)
{
XMLNODE *prev;

  if(node->children) {
    for(prev=node->children;prev->next;prev=prev->next)
      ;
    child->prev = prev;
    prev->next = child;
  } else {
    node->children = child;
  }
  child->parent = node;
}

void xml_add_sibling(TRANSFORM_CONTEXT *pctx, XMLNODE *node,XMLNODE *sibling)
{
  if(!node || !sibling)
    return;
  for(;node->next;node=node->next)
    ;
  sibling->prev = node;
  node->next = sibling;
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