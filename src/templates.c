/*
 *  TurboXSL XML+XSLT processing library
 *  Template precompiler / match routines
 *
 *
 *  (c) Egor Voznessenski, voznyak@mail.ru
 *
 *  $Id$
 *
**/
#include "templates.h"

#include <stdio.h>
#include <string.h>

#include "ltr_xsl.h"
#include "xsl_elements.h"

typedef struct template_ {
  XMLSTRING name;
  XMLSTRING match;
  XMLNODE *expr;
  unsigned int expression_weight;
  XMLNODE *content;
  struct template_ *next;
} template;

typedef struct template_tree_node_ {
    template *t;
    XMLDICT *children;
    struct template_tree_node_ *next;
} template_tree_node;

typedef struct template_tree_ {
    template_tree_node *head;
    template_tree_node *tail;
} template_tree;

typedef struct template_map_entry_ {
  template *head;
  template *root;
  template *always;
  template_tree *direct_match;
  struct template_map_entry_ *next;
} template_map_entry;

struct template_map_ {
  template_map_entry *empty_mode;
  template_map_entry *head;
  XMLDICT *modes;
};

template_map *template_map_create()
{
  template_map *result = memory_allocator_new(sizeof(template_map));
  result->empty_mode = memory_allocator_new(sizeof(template_map_entry));
  result->modes = dict_new(64);

  return result;
}

void template_map_release(template_map *map)
{
  template_map_entry *entry = map->head;
  while(entry != NULL)
  {
    if(entry->direct_match != NULL)
    {
      template_tree_node *node = entry->direct_match->head;
      while(node != NULL)
      {
        dict_free(node->children);
        node = node->next;
      }
    }
    entry = entry->next;
  }

  dict_free(map->modes);
}

template_map_entry *template_map_get_entry(template_map *map, XMLSTRING mode)
{
  if(mode == NULL) return map->empty_mode;

  template_map_entry *entry = (template_map_entry *) dict_find(map->modes, mode);
  if(entry == NULL)
  {
    entry = memory_allocator_new(sizeof(template_map_entry));
    dict_add(map->modes, mode, entry);
    if(map->head != NULL) entry->next = map->head;
    map->head = entry;
  }

  return entry;
}

template_map_entry *template_map_find_entry(template_map *map, XMLSTRING mode)
{
  if(mode == NULL) return map->empty_mode;
  return (template_map_entry *) dict_find(map->modes, mode);
}

template *template_map_find_template(template_map_entry *entry, XMLSTRING match)
{
  for(template *t = entry->head; t; t = t->next)
  {
    if(xmls_equals(t->match, match)) return t;
  }

  if(entry->direct_match != NULL)
  {
    template_tree_node *node = entry->direct_match->head;
    while(node != NULL)
    {
      if(node->t != NULL && xmls_equals(node->t->match, match)) return node->t;
      node = node->next;
    }
  }

  return NULL;
}

static void add_templ_match(TRANSFORM_CONTEXT *pctx, XMLNODE *content, char *match, XMLSTRING mode)
{
  // skip leading whitespace;
  while(x_is_ws(*match)) ++match;

  // trailing whitespace
  for(size_t r = strlen(match); r; --r)
  {
    if(x_is_ws(match[r-1]))
    {
      match[r-1] = 0;
    } 
    else
    {
      break;
    }
  }

  // if template match is empty, return empty node on match, not NULL
  if(!content) content = xml_new_node(pctx, NULL, EMPTY_NODE);

  XMLSTRING match_string = xmls_new_string_literal(match);
  template_map_entry *entry = template_map_get_entry(pctx->templates, mode);

  template *exists = template_map_find_template(entry, match_string);
  if(exists != NULL)
  {
    debug("add_templ_match:: found existing template");
    exists->content = content;
    return;
  }

  template *t = memory_allocator_new(sizeof(template));
  t->match = match_string;
  t->content = content;

  if(match[0] == '/' && match[1] == 0)
  {
    entry->root = t;
  }
  else if(match[0] == '*' && match[1] == 0)
  {
    entry->always = t;
  }
  else
  {
    XMLNODE *expression = xpath_find_expr(pctx, match_string);

    int is_direct_match = 1;
    unsigned int weight = 0;
    for(XMLNODE *n = expression; n; n = n->children)
    {
      if(expression->type != XPATH_NODE_ROOT && expression->type != XPATH_NODE_SELECT) is_direct_match = 0;
      weight += 1;
    }
    t->expression_weight = weight;

    if(is_direct_match)
    {
      if(entry->direct_match == NULL)
      {
        entry->direct_match = memory_allocator_new(sizeof(template_tree));
        template_tree_node *node = memory_allocator_new(sizeof(template_tree_node));
        entry->direct_match->head = node;
        entry->direct_match->tail = node;
      }

      template_tree_node *node = entry->direct_match->head;
      for(XMLNODE *n = expression; n; n = n->children)
      {
        if(n->type != XPATH_NODE_SELECT && n->type != XPATH_NODE_ROOT) continue;

        if(node->children == NULL) node->children = dict_new(16);

        XMLSTRING name = n->type == XPATH_NODE_SELECT ? n->name : xsl_s_slash;
        template_tree_node *child = dict_find(node->children, name);
        if(child == NULL)
        {
          child = memory_allocator_new(sizeof(template_tree_node));
          entry->direct_match->tail->next = child;
          entry->direct_match->tail = child;

          dict_add(node->children, name, child);
        }
        node = child;
      }
      node->t = t;

    }
    else
    {
      t->expr = expression;

      if(entry->head != NULL) t->next = entry->head;
      entry->head = t;
    }
  }
}

static void add_template(TRANSFORM_CONTEXT *pctx, XMLNODE *template, XMLSTRING name, XMLSTRING match, XMLSTRING mode)
{
  if(match) 
  {
    char *match_copy = xml_strdup(match->s);
    XMLNODE *content = template->children;
    while(match_copy)
    {
      if(strchr(match_copy,'['))
      {
        add_templ_match(pctx,content, match_copy, mode);
        match_copy = NULL;
      }
      else 
      {
        char *pp = strchr(match_copy,'|');
        if(pp) 
        {
          char *part_match = xml_new_string(match_copy, pp - match_copy);
          add_templ_match(pctx, content, part_match, mode);
          match_copy = pp + 1;
        }
        else 
        {
          add_templ_match(pctx,content, match_copy, mode);
          match_copy = NULL;
        }
      }
    }
  } 
  else 
  {
    if (dict_find(pctx->named_templ, name) != NULL)
    {
      debug("add_template:: replace template: %s", name->s);
      dict_replace(pctx->named_templ, name, template);
    }
    else
    {
      dict_add(pctx->named_templ, name, template);
    }
  }
}

int select_match_with_filter(TRANSFORM_CONTEXT *pctx, XMLNODE *locals, XMLNODE *node, XMLNODE *expr)
{
  if(node == NULL) return 0;

  if(expr->type == XPATH_NODE_INT) {
    long n = expr->extra.v.integer;
    return node->position == n;
  }

  RVALUE rv;
  xpath_execute_scalar(pctx, locals, expr, node, &rv);
  if(rv.type == VAL_INT) {
    long n = rv.v.integer;
    return node->position == n;
  }

  return rval2bool(&rv);
}

static int select_match(TRANSFORM_CONTEXT *pctx, XMLNODE *node, XMLNODE *templ)
{
  XMLNODE *t;
  RVALUE rv,rv1;
  int retVal = 0;

  if (templ == NULL) // XXX TODO is this corrrect?
    return 1;

  if (node == NULL)
    return 0;

  switch(templ->type) 
  {
    case XPATH_NODE_ROOT:
      return (pctx->root_node == node);


    case XPATH_NODE_SELECT:
      if(!xmls_equals(node->name, templ->name))
        return 0;

      return select_match(pctx, node->parent, templ->children);


    case XPATH_NODE_CONTEXT:
      return 1;


    case XPATH_NODE_SELF:
      return select_match(pctx, node, templ->children);


    case XPATH_NODE_FILTER:
      if(!select_match(pctx, node, templ->children))
        return 0;

      return select_match_with_filter(pctx, NULL, node, templ->children->next);


    case XPATH_NODE_UNION:
      for(t=templ->children;t;t=t->next) {
        if(select_match(pctx,node,t)) return 1;
      }
      return 0;


    case XPATH_NODE_ALL:
      return select_match(pctx, node->parent, templ->children);


    case XPATH_NODE_CALL:
      if(xmls_equals(templ->name, xsl_s_text))
        return node->type==TEXT_NODE;
      else if(xmls_equals(templ->name, xsl_s_node))
        return 1;

      return 0;


    case XPATH_NODE_EQ:
      xpath_execute_scalar(pctx, NULL, templ->children, node, &rv);
      xpath_execute_scalar(pctx, NULL, templ->children->next, node, &rv1);

      if (rval_equal(&rv, &rv1, 1))
        retVal = 1;

      rval_free(&rv);
      rval_free(&rv1);
      return retVal;


    case XPATH_NODE_NE:
      xpath_execute_scalar(pctx, NULL, templ->children, node, &rv);
      xpath_execute_scalar(pctx, NULL, templ->children->next, node, &rv1);

      if (rval_equal(&rv, &rv1, 0))
        retVal = 1;

      rval_free(&rv);
      rval_free(&rv1);
      return retVal;


    case XPATH_NODE_LE:
      xpath_execute_scalar(pctx, NULL, templ->children, node, &rv);
      xpath_execute_scalar(pctx, NULL, templ->children->next, node, &rv1);
      retVal = rval_less_or_equal(&rv, &rv1);
      rval_free(&rv);
      rval_free(&rv1);
      return retVal;


    case XPATH_NODE_GE:
      xpath_execute_scalar(pctx, NULL, templ->children, node, &rv);
      xpath_execute_scalar(pctx, NULL, templ->children->next, node, &rv1);
      retVal = rval_greater_or_equal(&rv, &rv1);
      rval_free(&rv);
      rval_free(&rv1);
      return retVal;


    case XPATH_NODE_LT:
      xpath_execute_scalar(pctx, NULL, templ->children, node, &rv);
      xpath_execute_scalar(pctx, NULL, templ->children->next, node, &rv1);
      retVal = rval_less(&rv, &rv1);
      rval_free(&rv);
      rval_free(&rv1);
      return retVal;


    case XPATH_NODE_GT:
      xpath_execute_scalar(pctx, NULL, templ->children, node, &rv);
      xpath_execute_scalar(pctx, NULL, templ->children->next, node, &rv1);
      retVal = rval_greater_or_equal(&rv, &rv1);
      rval_free(&rv);
      rval_free(&rv1);
      return retVal;

    case XPATH_NODE_NOT:
      xpath_execute_scalar(pctx, NULL, templ->children, node, &rv);
      retVal = rval2bool(&rv) ? 0 : 1;
      rval_free(&rv);
      return retVal;


    case XPATH_NODE_OR:
      for(t=templ->children;t;t=t->next) {
        if(select_match(pctx,node,t))
          return 1;
      }

      return 0;


    case XPATH_NODE_AND:
      for(t=templ->children;t;t=t->next) {
        if(!select_match(pctx,node,t))
          return 0;
      }

      return 1;


    default:
      return 0;
  }
}

template *find_select_template(TRANSFORM_CONTEXT *pctx, XMLNODE *node, template_map_entry *entry)
{
  template *result = NULL;
  unsigned int expression_weight = 0;

  if(entry->direct_match != NULL)
  {
    template_tree_node *parent = entry->direct_match->head;
    for(XMLNODE *n = node; n; n = n->parent)
    {
      XMLSTRING name = n->type == ELEMENT_NODE ? n->name : xsl_s_slash;
      template_tree_node *child = dict_find(parent->children, name);
      if(child == NULL) break;
      parent = child;
    }

    if(parent->t != NULL)
    {
      result = parent->t;
      expression_weight = parent->t->expression_weight;
    }
  }

  for(template *t = entry->head; t; t = t->next)
  {
    if(select_match(pctx, node, t->expr))
    {
      if(t->expression_weight > expression_weight)
      {
        expression_weight = t->expression_weight;
        result = t;
      }
    }
  }

  return result;
}

XMLNODE *find_template(TRANSFORM_CONTEXT *pctx, XMLNODE *node, XMLSTRING mode)
{
  template_map_entry *entry = template_map_find_entry(pctx->templates, mode);
  if(entry == NULL) return NULL;

  if(node == pctx->root_node && entry->root != NULL) return entry->root->content;

  template *template = find_select_template(pctx, node, entry);
  if (template != NULL) return template->content;

  if(node == pctx->root_node) return NULL;

  return entry->always != NULL ? entry->always->content : NULL;
}

XMLNODE *template_byname(TRANSFORM_CONTEXT *pctx, XMLSTRING name)
{
  if(name == NULL) return NULL;

  XMLNODE *template = (XMLNODE *) dict_find(pctx->named_templ, name);
  return template == NULL ? NULL : template->children;
}

void precompile_templates(TRANSFORM_CONTEXT *pctx, XMLNODE *node)
{
  for(;node;node=node->next) 
  {
    if(node->type==ELEMENT_NODE && xmls_equals(node->name, xsl_template))
    {
      XMLSTRING name  = xml_get_attr(node,xsl_a_name);
      XMLSTRING match = xml_get_attr(node,xsl_a_match);
      XMLSTRING mode  = xml_get_attr(node,xsl_a_mode);
      add_template(pctx, node, name, match, mode);
    }

    if(node->children)
    {
      precompile_templates(pctx, node->children);
    }
  }
}
