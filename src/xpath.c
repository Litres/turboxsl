/*
 *  TurboXSL XML+XSLT processing library
 *  XPATH language parser and evaluator
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
#include <math.h>

#include "ltr_xsl.h"

typedef struct {
  const char *value;
  char *p;
} XPATH_STRING;

static XMLNODE *do_or_expr(TRANSFORM_CONTEXT *pctx, XPATH_STRING *string);


XMLNODE *xpath_in_selection(XMLNODE *sel, char *name)
{
  for(;sel;sel=sel->next) {
    if(strcmp(sel->name, name) == 0) {
      return sel;
    }
  }
  return NULL;
}

void xpath_free_compiled(TRANSFORM_CONTEXT *pctx)
{
  free(pctx->compiled);
}

XMLNODE *xpath_find_expr(TRANSFORM_CONTEXT *pctx, char *expr)
{
  trace("xpath_find_expr:: expression: %s", expr);
  if (expr == NULL) return NULL;

  if (pthread_mutex_lock(&(pctx->lock))) {
    error("xpath_find_expr:: lock");
    return NULL;
  }

  XMLNODE *etree = NULL;
  unsigned i;
  for(i=0;i<pctx->n_exprs;++i) {
    if(strcmp(pctx->compiled[i].expr,expr) == 0) {
      etree = pctx->compiled[i].comp;
      break;
    }
  }

  if(etree==NULL) {
    etree = xpath_compile(pctx, expr);
    if(pctx->n_exprs>=pctx->m_exprs) {
      pctx->m_exprs += 100;
      pctx->compiled = realloc(pctx->compiled, pctx->m_exprs*sizeof(EXPTAB));
    }
    if(etree) {
      pctx->compiled[pctx->n_exprs].expr = expr;
      pctx->compiled[pctx->n_exprs].comp = etree;
      ++ pctx->n_exprs;
    }
  }

  if (pthread_mutex_unlock(&(pctx->lock))) {
    error("xpath_find_expr:: unlock");
  }

  return etree;
}

/*
 * initializes and frees RVALUEs. On free, frees allocated strings/nodesets. Type is set to NULL for reuse.
 *
 */

void add_node_str(XMLSTRING str, XMLNODE *node)
{
  for(;node;node=node->next) {
      if(node->children)
        add_node_str(str,node->children);
      if(node->type==TEXT_NODE)
        xmls_add_str(str,node->content);
  }
}

char *nodes2string(XMLNODE *node)
{
  XMLNODE     *tmp_node;
  char        *content = NULL;
  char    *tmp_content = NULL;
  size_t     content_len = 0;
  size_t tmp_content_len = 0;

  while (node) {
    tmp_node    = node->next;
    node->next  = NULL;
    tmp_content = node2string(node);
    node->next  = tmp_node;

    if (tmp_content)
    {
      tmp_content_len = strlen(tmp_content) + 1;
      char *old_content = content;
      content = memory_allocator_new(content_len + tmp_content_len);
      if (old_content != NULL) memcpy(content, old_content, content_len);
      memcpy(content + content_len, tmp_content, tmp_content_len);
      content_len = strlen(content);
    }

    node = node->next;
  }

  return content;
}

char *node2string(XMLNODE *node) 
{
  XMLSTRING str;

  if (node == NULL)
    return NULL;

  if (node->type == TEXT_NODE)
    return xml_strdup(node->content);
  str = xmls_new(100);
  add_node_str(str, node);

  return xmls_detach(str); //XXX leak!
}


XMLNODE *add_to_selection(XMLNODE *prev, XMLNODE *src, unsigned int *position)
{
  trace("add_to_selection:: previous: %s, source: %s", prev == NULL ? NULL : prev->name, src == NULL ? NULL : src->name);
  if(src==NULL)
    return prev;

  XMLNODE *dst = xml_new_node(NULL, NULL, src->type); //src->name already hashed, just copy pointer
  dst->name = src->name;
  dst->content = src->content;
  dst->attributes = src->attributes;
  dst->children = src->children;
  dst->parent = src->parent;
  dst->order = src->order;
  dst->uid = src->uid;
  dst->position = ++(*position);
  if(prev) {
    dst->prev = prev;
    prev->next = dst;
  }
  return dst;
}

XMLNODE *xml_add_siblings(XMLNODE *prev, XMLNODE *src, unsigned int *position, XMLNODE **psel) // NOTE: does not copy nodes, used for selection unions only!
{
  if(src==NULL)
    return prev;
  if(prev) {
    prev->next = src;
    src->prev = prev;
  } else if(psel) {
    *psel = src;
  }
  for(;src->next;src=src->next) {
    src->position = ++(*position);
  }
  return src;
}

XMLNODE *xml_append_node(XMLNODE *prev, XMLNODE *src)
{
  if(src == NULL) return prev;
  if(prev) {
    prev->next = src;
    src->prev = prev;
  }

  XMLNODE *r = src;
  while(r->next != NULL) r = r->next;

  return r;
}

XMLNODE *xpath_copy_nodeset(XMLNODE *set)
{
  XMLNODE *node = NULL;
  XMLNODE *tmp = NULL;
  unsigned int pos = 0;
  if(set == NULL)
    return NULL;
  for(;set;set=set->next) {
    tmp = add_to_selection(tmp, set, &pos);
    if(!node)
      node = tmp;
  }
  return node;
}


XMLNODE *xpath_filter(TRANSFORM_CONTEXT *pctx, XMLNODE *locals, XMLNODE *nodeset, XMLNODE *expr)
{
  XMLNODE *node = NULL;
  XMLNODE *tmp;
  RVALUE rv;
  unsigned int pos = 0;
  tmp = NULL;

  if (nodeset == NULL) return NULL;

  if (nodeset->type == EMPTY_NODE) nodeset = nodeset->children;
  
  if(expr->type == XPATH_NODE_INT) { // select n-th node from selection
    long n = expr->extra.v.integer;
    for(;nodeset;nodeset=nodeset->next) {
      if(nodeset->position==n) {
        tmp = add_to_selection(tmp,nodeset,&pos);
        if(!node) {
          node = tmp;
        }
      }
    }
  }
  else { // execute check expr for each node in nodeset, if true - add to new set
    for(;nodeset;nodeset=nodeset->next) {
      trace("xpath_filter:: node: %s", nodeset->name);
      xpath_execute_scalar(pctx, locals, expr, nodeset, &rv);

      if (rv.type == VAL_INT) 
      {
        long n = rv.v.integer;
        if (nodeset->position == n) {
          tmp = add_to_selection(tmp, nodeset, &pos);
          if(!node) {
            node = tmp;
            continue;
          }
        }
      }
      else {
        if(rval2bool(&rv))
        {
          tmp = add_to_selection(tmp,nodeset,&pos);
          if(!node) {
            // tmp = add_to_selection(tmp,nodeset,&pos);
            node = tmp;
          }
        }
      }
    }
  }

  return node;
}

int xpath_node_kind(XMLNODE *node, XMLNODE *kind)
{
  trace("xpath_node_kind:: node: %s (%s), kind: %s", node->name, nodeTypeNames[node->type], kind == NULL ? NULL : kind->name);
  if(kind == NULL) return node->type == ELEMENT_NODE;

  if(kind->type == XPATH_NODE_CALL)
  {
    if(strcmp(kind->name,"text") == 0)
    {
      return node->type == TEXT_NODE;
    }
    else if(strcmp(kind->name,"node") == 0)
    {
      // TODO
      return 1;
    }
    else
    {
      error("xpath_node_kind:: not supported kind: %s", kind->name);
      return 0;
    }
  }
  else
  {
    return node->type == ELEMENT_NODE && strcmp(node->name, kind->name) == 0;
  }
}

XMLNODE *add_all_children(XMLNODE *tmp, XMLNODE *node, unsigned int *pos, XMLNODE **head, XMLNODE *kind)
{
  for(;node;node=node->next) {
    trace("add_all_children:: child: %s (%s)", node->name, nodeTypeNames[node->type]);
    if(node->type == EMPTY_NODE) {
      tmp = add_all_children(tmp,node->children,pos,head,kind);
      continue;
    }

    if(xpath_node_kind(node, kind)) {
      tmp = add_to_selection(tmp,node,pos);
      if(!(*head)) *head = tmp;
    }

    if(node->children)
      tmp = add_all_children(tmp,node->children,pos,head,kind);
  }
  return tmp;
}

XMLNODE *xpath_get_self(XMLNODE *current, XMLNODE *kind)
{
  trace("xpath_get_self:: current: %s", current->name);
  unsigned pos = 0;
  return xpath_node_kind(current, kind) ? add_to_selection(NULL, current, &pos) : NULL;
}

XMLNODE *xpath_get_parent(XMLNODE *current, XMLNODE *kind)
{
  trace("xpath_get_parent:: current: %s", current->name);
  XMLNODE *parent = current->parent;
  if (parent == NULL || strcmp(parent->name, "root") == 0) return NULL;

  unsigned pos = 0;
  return xpath_node_kind(parent, kind) ? add_to_selection(NULL, parent, &pos) : NULL;
}

XMLNODE *xpath_get_child(XMLNODE *current, XMLNODE *kind)
{
  trace("xpath_get_child:: current: %s", current->name);
  XMLNODE *tmp = NULL;
  XMLNODE *ret = NULL;
  unsigned pos = 0;
  for(XMLNODE *node=current->children;node;node=node->next) {
    if(xpath_node_kind(node, kind)) {
      tmp = add_to_selection(tmp,node,&pos);
      if(!ret) ret = tmp;
    }
  }
  return ret;
}

XMLNODE *xpath_get_descendant(XMLNODE *current, XMLNODE *kind)
{
  trace("xpath_get_descendant:: current: %s", current->name);
  XMLNODE *ret = NULL;
  unsigned pos = 0;
  add_all_children(NULL, current->children, &pos, &ret, kind);
  return ret;
}

XMLNODE *xpath_get_descendant_or_self(XMLNODE *current, XMLNODE *kind)
{
  trace("xpath_get_descendant_or_self:: current: %s", current->name);
  XMLNODE *result = xpath_get_self(current, kind);

  XMLNODE *descendant = xpath_get_descendant(current, kind);
  if (descendant != NULL)
  {
    if (result == NULL) result = descendant;
    else
    {
      descendant->prev = result;
      result->next = descendant;
    }
  }

  return result;
}

XMLNODE *xpath_get_all(XMLNODE *current, XMLNODE *kind)
{
  trace("xpath_get_all:: current: %s", current->name);
  return xpath_get_child(current, kind);
}

XMLNODE *xpath_get_ancestor(XMLNODE *current, XMLNODE *kind)
{
  trace("xpath_get_ancestor:: current: %s", current->name);
  XMLNODE *tmp = NULL;
  XMLNODE *ret = NULL;
  unsigned pos = 0;
  for(XMLNODE *node=current->parent;node;node=node->parent) {
    if (strcmp(node->name, "root") == 0) return ret;
    if(xpath_node_kind(node, kind)) {
      tmp = add_to_selection(tmp,node,&pos);
      if(!ret) ret = tmp;
    }
  }
  return ret;
}

XMLNODE *xpath_get_ancestor_or_self(XMLNODE *current, XMLNODE *kind)
{
  trace("xpath_get_ancestor_or_self:: current: %s", current->name);
  XMLNODE *result = xpath_get_self(current, kind);

  XMLNODE *ancestor = xpath_get_ancestor(current, kind);
  if (ancestor != NULL)
  {
    if (result == NULL) result = ancestor;
    else
    {
      ancestor->prev = result;
      result->next = ancestor;
    }
  }

  return result;
}

XMLNODE *xpath_get_preceding_sibling(XMLNODE *current, XMLNODE *kind)
{
  trace("xpath_get_preceding_sibling:: current: %s", current->name);
  XMLNODE *tmp = NULL;
  XMLNODE *ret = NULL;
  unsigned pos = 0;
  for(XMLNODE *node=current->prev;node;node=node->prev) {
    if(xpath_node_kind(node, kind)) {
      tmp = add_to_selection(tmp,node,&pos);
      if(!ret) ret = tmp;
    }
  }
  return ret;
}

XMLNODE *xpath_get_preceding(XMLNODE *current, XMLNODE *kind)
{
  trace("xpath_get_preceding:: current: %s", current->name);
  XMLNODE *tmp = NULL;
  XMLNODE *ret = NULL;
  unsigned pos = 0;
  for(XMLNODE *node=current->prev;node;node=node->prev) {
    if(xpath_node_kind(node, kind)) {
      tmp = add_to_selection(tmp,node,&pos);
      if(!ret) ret = tmp;
    }
    XMLNODE *descendant = xpath_get_descendant(node, kind);
    if(descendant != NULL) {
      tmp = xml_append_node(tmp, descendant);
      if(!ret) ret = tmp;
    }
  }
  return ret;
}

XMLNODE *xpath_get_following_sibling(XMLNODE *current, XMLNODE *kind)
{
  trace("xpath_get_following_sibling:: current: %s", current->name);
  XMLNODE *tmp = NULL;
  XMLNODE *ret = NULL;
  unsigned pos = 0;
  for(XMLNODE *node=current->next;node;node=node->next) {
    if(xpath_node_kind(node, kind)) {
      tmp = add_to_selection(tmp,node,&pos);
      if(!ret) ret = tmp;
    }
  }
  return ret;
}

XMLNODE *xpath_get_following(XMLNODE *current, XMLNODE *kind)
{
  trace("xpath_get_following:: current: %s", current->name);
  XMLNODE *tmp = NULL;
  XMLNODE *ret = NULL;
  unsigned pos = 0;
  for(XMLNODE *node=current->next;node;node=node->next) {
    if(xpath_node_kind(node, kind)) {
      tmp = add_to_selection(tmp,node,&pos);
      if(!ret) ret = tmp;
    }
    XMLNODE *descendant = xpath_get_descendant(node, kind);
    if(descendant != NULL)
    {
      tmp = xml_append_node(tmp, descendant);
      if(!ret) ret = tmp;
    }
  }
  return ret;
}

XMLNODE *xpath_apply_selector(XMLNODE *set, XMLNODE *etree, XMLNODE * (*selector)(XMLNODE *, XMLNODE *))
{
  XMLNODE *head = NULL;
  XMLNODE *tail = NULL;
  for(XMLNODE *node=set;node;node=node->next) {
    trace("xpath_apply_selector:: node: %s", node->name);
    XMLNODE *tmp = selector(node, etree->attributes);
    if (tmp == NULL) continue;
    if (head == NULL) {
      head = tmp;
      tail = tmp;
    } else {
      tail->next = tmp;
    }
    while (tail->next != NULL) tail = tail->next;
  }
  return head;
}

void xpath_select_common(TRANSFORM_CONTEXT *pctx, XMLNODE *locals, XMLNODE *etree, XMLNODE *current, RVALUE *res, XMLNODE * (*selector)(XMLNODE *, XMLNODE *))
{
  trace("xpath_select_common:: current: %s", current->name);
  res->type = VAL_NODESET;
  if(etree->children) {
    trace("xpath_select_common:: has child XPath expression");
    RVALUE rv;
    xpath_execute_scalar(pctx,locals,etree->children,current,&rv);
    if(rv.type == VAL_NODESET) {
      res->v.nodeset = xpath_apply_selector(rv.v.nodeset, etree, selector);
    } else {
      // can not select from non-nodeset
      res->v.nodeset = NULL;
    }
    rval_free(&rv);
    return;
  }

  res->v.nodeset = selector(current, etree->attributes);
}

XMLNODE *xpath_get_attrs(XMLNODE *nodeset)
{
  XMLNODE *tmp = NULL;
  XMLNODE *head = NULL;
  XMLNODE *attr;
  unsigned pos = 0;

  for(;nodeset;nodeset=nodeset->next) {
    trace("xpath_get_attrs:: node: %s", nodeset->name);
    for(attr=nodeset->attributes;attr;attr=attr->next) {
      trace("xpath_get_attrs:: attribute: %s", attr->name);
      tmp = add_to_selection(tmp,attr,&pos);
      if(!head)
        head = tmp;
    }
  }
  return head;
}

XMLNODE *xpath_sort_selection(TRANSFORM_CONTEXT *pctx, XMLNODE *locals, XMLNODE *selection, XMLNODE *sort)
{
  XMLNODE *newsel;
  char *t;
  unsigned i,n,again;
  int numeric_sort = (sort->flags&XML_FLAG_SORTNUMBER)?1:0;
  int backward_sort = (sort->flags&XML_FLAG_DESCENDING)?-1:1;
  double *nsk = NULL;
  double dt;

  if(selection==NULL||selection->next==NULL)  //do not attempt to sort single-element or empty nodesets;
    return selection;

  if (pthread_mutex_lock(&(pctx->lock))) {
    error("xpath_sort_selection:: lock");
    return selection;
  }

  newsel = selection;
  for(n=0;selection;selection=selection->next) { //first, count selection length and allocate tables
    ++n;
  }
  debug("xpath_sort_selection:: sorting %d nodes", n);
  if(pctx->sort_size<n) {
    pctx->sort_size = n*2;
    pctx->sort_keys = (char **)realloc(pctx->sort_keys, pctx->sort_size*sizeof(char *));
    pctx->sort_nodes = (XMLNODE **)realloc(pctx->sort_nodes, pctx->sort_size*sizeof(XMLNODE *));
  }
  if(numeric_sort) {
    nsk = malloc(sizeof(double)*n);
  }
  selection = newsel;
  for(i=0;i<n;++i) {
    pctx->sort_nodes[i] = selection;
    pctx->sort_keys[i] = sort->compiled?xpath_eval_string(pctx,locals,selection,sort->compiled):node2string(selection);
    if(numeric_sort) {
      if(pctx->sort_keys[i])
        nsk[i] = strtod(pctx->sort_keys[i],NULL);
      else
        nsk[i]=-9999999999;
    }
    selection = selection->next;
  }

  if(numeric_sort) {
    for(again=1;again;) { // do sorting
      again = 0;
      for(i=1;i<n;++i) {
        if(backward_sort*(nsk[i-1]-nsk[i])>0.0) {
          selection = pctx->sort_nodes[i];
          pctx->sort_nodes[i]=pctx->sort_nodes[i-1];
          pctx->sort_nodes[i-1]=selection;
          dt = nsk[i];
          nsk[i]=nsk[i-1];
          nsk[i-1]=dt;
          again = 1;
        }
      }
    }
  } else {
    for(again=1;again;) { // do sorting
      again = 0;
      for(i=1;i<n;++i) {
        if(backward_sort*xml_strcasecmp(pctx->sort_keys[i-1],pctx->sort_keys[i])>0) {
          selection = pctx->sort_nodes[i];
          pctx->sort_nodes[i]=pctx->sort_nodes[i-1];
          pctx->sort_nodes[i-1]=selection;
          t = pctx->sort_keys[i];
          pctx->sort_keys[i]=pctx->sort_keys[i-1];
          pctx->sort_keys[i-1]=t;
          again = 1;
        }
      }
    }
  }

  pctx->sort_nodes[0]->prev=NULL;
  for(i=1;i<n;++i) {
    pctx->sort_nodes[i-1]->position = i;
    pctx->sort_nodes[i]->prev = pctx->sort_nodes[i-1];
    pctx->sort_nodes[i-1]->next = pctx->sort_nodes[i];
  }
  pctx->sort_nodes[n-1]->next = NULL;
  pctx->sort_nodes[n-1]->position = n;
  newsel = pctx->sort_nodes[0];
  free(nsk);

  if (pthread_mutex_unlock(&(pctx->lock))) {
    error("xpath_sort_selection:: unlock");
  }

  return newsel;
}


void xpath_execute_scalar(TRANSFORM_CONTEXT *pctx, XMLNODE *locals, XMLNODE *etree, XMLNODE *current, RVALUE *res)
{
  XMLNODE *expr;
  XMLNODE *tmp;
  XMLNODE *selection;
  RVALUE rv,rv1;
  char *name;
  double l,r;
  unsigned pos = 0;

  if(!etree) {
    debug("xpath_execute_scalar:: etree is NULL");
    res->type = VAL_NULL;
    return;
  }

  trace("xpath_execute_scalar:: type: %s", nodeTypeNames[etree->type]);

  switch(etree->type) {
    case XPATH_NODE_INT:
      res->type = VAL_INT;
      res->v.integer = etree->extra.v.integer;
      return;

    case XPATH_NODE_UNION:
      selection = NULL;
      tmp = NULL;
      for(etree=etree->children;etree;etree=etree->next) {
        rv.type = VAL_NULL;
        xpath_execute_scalar(pctx,locals,etree,current,&rv);
        if(rv.type == VAL_NODESET) {
          tmp = xml_add_siblings(tmp,rv.v.nodeset,&pos,&selection);
        } else {
          name = rval2string(&rv);
          if(!name)
            continue;
          expr = xml_new_node(pctx, NULL,TEXT_NODE);
          expr->content = name;
          tmp = xml_add_siblings(tmp,expr,&pos,&selection);
        }
      }
      res->type = VAL_NODESET;
      res->v.nodeset = selection;
      return;

    case XPATH_NODE_ATTR_ALL:
      res->type = VAL_NODESET;
      if(etree->children) {
        xpath_execute_scalar(pctx,locals,etree->children,current,&rv); // selection
        if(rv.type == VAL_NODESET) {
          selection = xpath_get_attrs(rv.v.nodeset);
          res->v.nodeset = selection;
        } else {  // can not select from non-nodeset
          res->v.nodeset = NULL;
        }
        rval_free(&rv);
      } else {   // no children -- select attributes from context node
        selection = xpath_get_attrs(current);
        res->v.nodeset = selection;
      }
      return;

    case XPATH_NODE_ROOT:
      selection = add_to_selection(NULL,pctx->root_node,&pos);
      res->type = VAL_NODESET;
      res->v.nodeset = selection;
      return;

    case XPATH_NODE_CONTEXT:
      selection = add_to_selection(NULL,current,&pos);
      res->type = VAL_NODESET;
      res->v.nodeset = selection;
      return;

    case XPATH_NODE_FILTER:
      xpath_execute_scalar(pctx,locals,etree->children,current,&rv); // selection
      res->type = VAL_NODESET;
      if(rv.type == VAL_NODESET) 
      {
        selection = xpath_filter(pctx,locals,rv.v.nodeset,etree->children->next);
        res->v.nodeset = selection;
      }
      else if(rv.type == VAL_STRING)
      {
        // create element node with attribute string value
        XMLNODE *attribute = xml_new_node(pctx, NULL, ELEMENT_NODE);
        attribute->children = xml_new_node(pctx, NULL, TEXT_NODE);
        attribute->children->content = rv.v.string;
        if(xpath_filter(pctx,locals,attribute,etree->children->next) != NULL)
        {
          res->type = VAL_STRING;
          res->v.string = rv.v.string;
        } else {
          res->v.nodeset = NULL;
        }
      }
      else {  // can not select from non-nodeset
        res->v.nodeset = NULL;
      }
      rval_free(&rv);
      return;

    case XPATH_NODE_ALL:
      xpath_select_common(pctx, locals, etree, current, res, xpath_get_all);
      return;

    case XPATH_NODE_SELECT:
      xpath_select_common(pctx, locals, etree, current, res, xpath_get_child);
      return;

    case XPATH_NODE_PARENT:
      xpath_select_common(pctx, locals, etree, current, res, xpath_get_parent);
      return;

    case XPATH_NODE_SELF:
      xpath_select_common(pctx, locals, etree, current, res, xpath_get_self);
      return;

    case XPATH_NODE_DESCENDANT:
      xpath_select_common(pctx, locals, etree, current, res, xpath_get_descendant);
      return;

    case XPATH_NODE_DESCENDANT_OR_SELF:
      xpath_select_common(pctx, locals, etree, current, res, xpath_get_descendant_or_self);
      return;

    case XPATH_NODE_ANCESTOR:
      xpath_select_common(pctx, locals, etree, current, res, xpath_get_ancestor);
      return;

    case XPATH_NODE_ANCESTOR_OR_SELF:
      xpath_select_common(pctx, locals, etree, current, res, xpath_get_ancestor_or_self);
      return;

    case XPATH_NODE_PRECEDING_SIBLING:
      xpath_select_common(pctx, locals, etree, current, res, xpath_get_preceding_sibling);
      return;

    case XPATH_NODE_PRECEDING:
      xpath_select_common(pctx, locals, etree, current, res, xpath_get_preceding);
      return;

    case XPATH_NODE_FOLLOWING_SIBLING:
      xpath_select_common(pctx, locals, etree, current, res, xpath_get_following_sibling);
      return;

    case XPATH_NODE_FOLLOWING:
      xpath_select_common(pctx, locals, etree, current, res, xpath_get_following);
      return;

    case XPATH_NODE_OR:
      for(expr=etree->children;expr;expr=expr->next) {
        xpath_execute_scalar(pctx,locals,expr,current,&rv);
        if(rval2bool(&rv)) {
          res->type=VAL_BOOL;
          res->v.integer=1;
          return;
        }
      }
      res->type=VAL_BOOL;
      res->v.integer=0;
      return;

    case XPATH_NODE_AND:
      for(expr=etree->children;expr;expr=expr->next) {
        xpath_execute_scalar(pctx,locals,expr,current,&rv);
        if(!rval2bool(&rv)) {
          res->type=VAL_BOOL;
          res->v.integer=0;
          return;
        }
      }
      res->type=VAL_BOOL;
      res->v.integer=1;
      return;

    case XPATH_NODE_EQ:
      xpath_execute_scalar(pctx,locals,etree->children,current,&rv);
      xpath_execute_scalar(pctx,locals,etree->children->next,current,&rv1);

      res->type = VAL_BOOL;
      if(rval_equal(&rv, &rv1, 1)) {
        res->v.integer = 1;
      } 
      else {
      	res->v.integer = 0;
      }

      rval_free(&rv);
      rval_free(&rv1);
      return;

    case XPATH_NODE_NE:
      xpath_execute_scalar(pctx,locals,etree->children,current,&rv);
      xpath_execute_scalar(pctx,locals,etree->children->next,current,&rv1);
      res->type=VAL_BOOL;
      if(rval_equal(&rv, &rv1, 0)) {
      	res->v.integer=1;
      } else {
      	res->v.integer=0;
      }
      rval_free(&rv);
      rval_free(&rv1);
      return;

    case XPATH_NODE_LT:
      xpath_execute_scalar(pctx,locals,etree->children,current,&rv);
      xpath_execute_scalar(pctx,locals,etree->children->next,current,&rv1);
      res->type=VAL_BOOL;
      if(0>rval_compare(&rv, &rv1)) {
      	res->v.integer=1;
      } else {
      	res->v.integer=0;
      }
      rval_free(&rv);
      rval_free(&rv1);
      return;

    case XPATH_NODE_LE:
      xpath_execute_scalar(pctx,locals,etree->children,current,&rv);
      xpath_execute_scalar(pctx,locals,etree->children->next,current,&rv1);
      res->type=VAL_BOOL;
      if(0>=rval_compare(&rv, &rv1)) {
      	res->v.integer=1;
      } else {
      	res->v.integer=0;
      }
      rval_free(&rv);
      rval_free(&rv1);
      return;

    case XPATH_NODE_GT:
      xpath_execute_scalar(pctx,locals,etree->children,current,&rv);
      xpath_execute_scalar(pctx,locals,etree->children->next,current,&rv1);
      res->type=VAL_BOOL;
      if(0<rval_compare(&rv, &rv1)) {
      	res->v.integer=1;
      } else {
      	res->v.integer=0;
      }
      rval_free(&rv);
      rval_free(&rv1);
      return;

    case XPATH_NODE_GE:
      xpath_execute_scalar(pctx,locals,etree->children,current,&rv);
      xpath_execute_scalar(pctx,locals,etree->children->next,current,&rv1);
      res->type=VAL_BOOL;
      if(0<=rval_compare(&rv, &rv1)) {
      	res->v.integer=1;
      } else {
      	res->v.integer=0;
      }
      rval_free(&rv);
      rval_free(&rv1);
      return;

    case XPATH_NODE_ADD:
      xpath_execute_scalar(pctx,locals,etree->children,current,&rv);
      xpath_execute_scalar(pctx,locals,etree->children->next,current,&rv1);
      l = rval2number(&rv);
      r = rval2number(&rv1);
      res->type = VAL_NUMBER;
      res->v.number = l+r;
      return;

    case XPATH_NODE_SUB:
      xpath_execute_scalar(pctx,locals,etree->children,current,&rv);
      xpath_execute_scalar(pctx,locals,etree->children->next,current,&rv1);
      l = rval2number(&rv);
      r = rval2number(&rv1);
      res->type = VAL_NUMBER;
      res->v.number = l-r;
      return;

    case XPATH_NODE_MUL:
      xpath_execute_scalar(pctx,locals,etree->children,current,&rv);
      xpath_execute_scalar(pctx,locals,etree->children->next,current,&rv1);
      l = rval2number(&rv);
      r = rval2number(&rv1);
      res->type = VAL_NUMBER;
      res->v.number = l*r;
      return;

    case XPATH_NODE_DIV:
      xpath_execute_scalar(pctx,locals,etree->children,current,&rv);
      xpath_execute_scalar(pctx,locals,etree->children->next,current,&rv1);
      l = rval2number(&rv);
      r = rval2number(&rv1);
      res->type = VAL_NUMBER;
      res->v.number = l/r;
      return;

    case XPATH_NODE_MOD:
      xpath_execute_scalar(pctx,locals,etree->children,current,&rv);
      xpath_execute_scalar(pctx,locals,etree->children->next,current,&rv1);
      l = rval2number(&rv);
      r = rval2number(&rv1);
      {
        long ll = trunc(l);
        long lr = trunc(r);
        res->type = VAL_NUMBER;
        res->v.number = (ll%lr);
      }
      return;

    case XPATH_NODE_NOT:
      xpath_execute_scalar(pctx,locals,etree->children,current,&rv);
      res->type=VAL_BOOL;
      res->v.integer=rval2bool(&rv)?0:1;
      return;

    case TEXT_NODE:
      res->type=VAL_STRING;
      res->v.string = xml_strdup(etree->content);
      return;

    case XPATH_NODE_CALL:
      res->type=VAL_NULL;
      xpath_call_dispatcher(pctx,locals,etree->name,etree->children,current,res);
      return;

    case XPATH_NODE_ATTR:
      rv.type = VAL_NULL;
      if(etree->children) 
      {
        xpath_execute_scalar(pctx,locals,etree->children,current,&rv);
        if(rv.type == VAL_NODESET) 
        {
          XMLNODE *r = NULL;
          XMLNODE *t = NULL;
          if(rv.v.nodeset && !rv.v.nodeset->next) {
            name = xml_get_attr(rv.v.nodeset,etree->name);
            if(name) {
              res->type = VAL_STRING;
              res->v.string = xml_strdup(name);
            } else {
              res->type = VAL_NULL;
            }
            rval_free(&rv);
            return;
          }
          for(current = rv.v.nodeset;current;current=current->next) {
            name = xml_get_attr(current,etree->name);
            if(name) {
              expr = xml_new_node(pctx, NULL,TEXT_NODE);
              expr->content = name;
              t = add_to_selection(t, expr, &pos);
              if (!r) 
                r = t;
            }
          }
          res->type = VAL_NODESET;
          res->v.nodeset = r;
        } else {
          res->type = VAL_NULL;
        }
        rval_free(&rv);
      }
      else {
        name=current?xml_get_attr(current,etree->name):NULL;
        if(name) {
          res->type = VAL_STRING;
          res->v.string = xml_strdup(name);
        } else {
          res->type = VAL_NULL;
        }
      }
      return;

    case XPATH_NODE_VAR:
      res->type =VAL_NULL;
      if(!current)
        return;
      get_variable_rv(pctx,locals,etree->name,res);
      return;
  }
  res->type = VAL_NULL;
}



int xpath_eval_boolean(TRANSFORM_CONTEXT *pctx, XMLNODE *locals, XMLNODE *current, XMLNODE *etree)
{
  RVALUE rval;

  if(etree) {
    locals->parent = current;
    xpath_execute_scalar(pctx,locals,etree,current,&rval);
    return rval2bool(&rval);
  }
  return 0;
}

char *xpath_eval_string(TRANSFORM_CONTEXT *pctx, XMLNODE *locals, XMLNODE *current, XMLNODE *etree)
{
  RVALUE rval;
  char     *s;

  rval_init(&rval);

  if(etree) 
  {
    locals->parent = current;
    xpath_execute_scalar(pctx,locals,etree,current,&rval);
    s = rval2string(&rval);
    return s;
  }

  return NULL;
}

void xpath_eval_expression(TRANSFORM_CONTEXT *pctx, XMLNODE *locals, XMLNODE *current, char *expr, RVALUE *res)
{
  XMLNODE *etree;
  rval_init(res);
  etree = xpath_find_expr(pctx, expr);
  if(etree) {
    locals->parent = current;
    xpath_execute_scalar(pctx,locals,etree,current,res);
  }
}

XMLNODE *xpath_eval_selection(TRANSFORM_CONTEXT *pctx, XMLNODE *locals, XMLNODE *current, XMLNODE *etree)
{
  RVALUE rval;
  rval_init(&rval);

  if(etree) {
    locals->parent = current;
    xpath_execute_scalar(pctx,locals,etree,current,&rval);
    if(rval.type == VAL_NODESET) {
      return rval.v.nodeset;
    }
  }
  return NULL;
}


void xpath_eval_node(TRANSFORM_CONTEXT *pctx, XMLNODE *locals, XMLNODE *current, char *expr, RVALUE *this)
{
  XMLNODE *etree;

  rval_free(this);

  etree = xpath_find_expr(pctx, expr);
  if (etree) 
  {
    locals->parent = current;
    xpath_execute_scalar(pctx, locals, etree, current, this);
  }
}


/*
 * nodeset_free - frees nodeset if non-empty;
 *
 */

void xpath_free_selection(TRANSFORM_CONTEXT *pctx, XMLNODE *sel)
{
    // TODO
}


/*
 * XPath expression compilation
 *
 */

static void skip_ws(XPATH_STRING *string)
{
  char c;
  for(;;) {
    c = *string->p;
    if(!c)
      return;
    if(!x_is_ws(c))
      return;
    (string->p)++;
  }
}

int match(char **eptr, char *str)
{
  char *pos = *eptr;
  while(*str) {
    if(**eptr != *str) {
      if(!(*str == ' ' && (x_is_ws(**eptr)||**eptr=='('))) {
        *eptr = pos;
        return 0;
      }
    }
    ++str;
    ++(*eptr);
  }
  return 1;
}


static
XMLNODE *do_var_expr(TRANSFORM_CONTEXT *pctx, XPATH_STRING *string, NODETYPE t)
{
  XMLNODE *ret;
  char *p, *e;

  p = string->p;
  for (e = p+1; *e && x_is_namechar(*e); ++e)
    ;
  ret = xml_new_node(pctx, NULL, t);
  ret->name=xml_new_string(p, e - p);
  trace("do_var_expr:: variable name: %s", ret->name);
  string->p=e;
  skip_ws(string);
  return ret;
}


static
XMLNODE *do_select_expr(TRANSFORM_CONTEXT *pctx, XPATH_STRING *string)
{
  trace("do_select_expr:: expression: %s", string->p);
  XMLNODE *result = NULL;

  skip_ws(string);
  if(*string->p=='*')
  {
    ++(string->p);
    return xml_new_node(pctx,NULL,XPATH_NODE_ALL);
  } 
  else if(*string->p=='.')
  {
    ++(string->p);
    if(*string->p=='.')
    {
      ++(string->p);
      return xml_new_node(pctx,NULL,XPATH_NODE_PARENT);
    } 
    else 
    {
      return xml_new_node(pctx,NULL,XPATH_NODE_SELF);
    }
  } 
  else if(*string->p=='/')
  {
    if(*(string->p + 1) == '*') (string->p) += 2;
    return xml_new_node(pctx,NULL, XPATH_NODE_DESCENDANT);
  }

  char *p = (string->p);
  if(*p=='@') 
  {
    ++p;
    if(*p=='*') 
    {
      ++p;
      string->p = p;
      return xml_new_node(pctx,NULL,XPATH_NODE_ATTR_ALL);
    } 
    else
      result = xml_new_node(pctx,NULL,XPATH_NODE_ATTR);
  } 
  else 
  {
    if(match(&(string->p),"child::"))
    {
      result = xml_new_node(pctx,NULL,XPATH_NODE_SELECT);
    }
    else if(match(&(string->p),"descendant::"))
    {
      result = xml_new_node(pctx,NULL,XPATH_NODE_DESCENDANT);
    }
    else if(match(&(string->p),"attribute::"))
    {
      result = xml_new_node(pctx,NULL,XPATH_NODE_ATTR);
    }
    else if(match(&(string->p),"self::"))
    {
      result = xml_new_node(pctx,NULL,XPATH_NODE_SELF);
    }
    else if(match(&(string->p),"descendant-or-self::"))
    {
      result = xml_new_node(pctx,NULL,XPATH_NODE_DESCENDANT_OR_SELF);
    }
    else if(match(&(string->p),"following-sibling::"))
    {
      result = xml_new_node(pctx,NULL,XPATH_NODE_FOLLOWING_SIBLING);
    }
    else if(match(&(string->p),"following::"))
    {
      result = xml_new_node(pctx,NULL,XPATH_NODE_FOLLOWING);
    }
    else if(match(&(string->p),"parent::"))
    {
      result = xml_new_node(pctx,NULL,XPATH_NODE_PARENT);
    }
    else if(match(&(string->p),"ancestor::"))
    {
      result = xml_new_node(pctx,NULL,XPATH_NODE_ANCESTOR);
    }
    else if(match(&(string->p),"preceding-sibling::"))
    {
      result = xml_new_node(pctx,NULL,XPATH_NODE_PRECEDING_SIBLING);
    }
    else if(match(&(string->p),"preceding::"))
    {
      result = xml_new_node(pctx,NULL,XPATH_NODE_PRECEDING);
    }
    else if(match(&(string->p),"ancestor-or-self::"))
    {
      result = xml_new_node(pctx,NULL,XPATH_NODE_ANCESTOR_OR_SELF);
    }

    if (result != NULL)
    {
      if(*string->p=='*')
      {
        trace("do_select_expr:: wildcard");
        ++(string->p);
        result->name = NULL;
        return result;
      }

      p = string->p;

      char *e;
      for (e = p+1; *e && x_is_namechar(*e); ++e)
        ;

      char *name = xml_new_string(p, e - p);
      string->p=e;

      if(*string->p=='(')
      {
        trace("do_select_expr:: kind test: %s", name);
        while (*string->p!=')') ++(string->p);

        result->attributes = xml_new_node(pctx,name,XPATH_NODE_CALL);
      }
      else
      {
        trace("do_select_expr:: name: %s", name);
        result->name = name;
        result->attributes = xml_new_node(pctx,name,EMPTY_NODE);
      }

      return result;
    }
  }

  char *e;
  for (e = p+1; *e && x_is_namechar(*e); ++e)
      ;

  char *name = xml_new_string(p, e - p);
  string->p=e;

  skip_ws(string);
  if(*string->p=='(') { // call function
    ++(string->p);
    XMLNODE *node = xml_new_node(pctx,name,XPATH_NODE_CALL);
    for(;;) {
      skip_ws(string);
      if(*string->p==')') {
        ++ (string->p);
        break;
      }
      XMLNODE *argument = do_or_expr(pctx,string);
      xml_add_child(pctx, node, argument);
      skip_ws(string);
      if(*string->p==',')
        ++(string->p);
    }
    trace("do_select_expr:: call: %s", name);
    return node;
  }

  if (result == NULL)
  {
    trace("do_select_expr:: select");
    result = xml_new_node(pctx,name,XPATH_NODE_SELECT);
    result->attributes = xml_new_node(pctx,name,EMPTY_NODE);
  }
  else
  {
    result->name = name;
  }

  return result;
}


static
XMLNODE *do_node2_expr(TRANSFORM_CONTEXT *pctx, XPATH_STRING *string)
{
  trace("do_node2_expr:: expression: %s", string->p);
  XMLNODE *node1;
  skip_ws(string);
  if(*string->p=='\'')
  {
    char *p, *e;
    p = ++(string->p);
    node1 = xml_new_node(pctx,NULL,TEXT_NODE);
    for(e=p;*e && *e != '\'';++e)
        ;
    node1->content = memory_allocator_new((e - p) + 2);
    memcpy(node1->content,p,e-p);
    node1->content[e-p]=0;
    if(*e)++e;
    string->p = e;
    skip_ws(string);
    return node1;
  } 
  else if(*string->p=='(')
  {
    ++(string->p);
    node1 = do_or_expr(pctx, string);
    skip_ws(string);
    if(*string->p==')')
    {
      ++(string->p);
      skip_ws(string);
    }
    return node1;
  } 
  else if (x_can_number(string->p))
  {
    char *p, *e;
    int f_fl = 0;
    p = (string->p);
    node1 = xml_new_node(pctx,NULL,TEXT_NODE);
    for(e=p+1;*e>='0' && *e <= '9';++e)
        ;
    if(*e=='.') 
    {
      ++e;
      f_fl = 1;
    }
    for(;*e>='0' && *e <= '9';++e)
        ;
    if(f_fl) 
    {
      node1->content = memory_allocator_new(e - p + 2);
      memcpy(node1->content,p,e-p);
      node1->content[e-p]=0;
    } 
    else 
    {
      size_t number_size = e - p + 2;
      char *number = memory_allocator_new(number_size);
      memcpy(number, p, e - p);

      node1->type = XPATH_NODE_INT;
      node1->extra.type = VAL_INT;
      node1->extra.v.integer = atol(number);
    }
    string->p = e;
    skip_ws(string);
    return node1;
  } 
  else if (*string->p == '$')
  {
    ++(string->p);
    return do_var_expr(pctx,string,XPATH_NODE_VAR);
  } 
  else if (*string->p == '@')
  {
    ++(string->p);
    if(*string->p=='*')
    {
      ++(string->p);
      return xml_new_node(pctx,NULL,XPATH_NODE_ATTR_ALL);
    } 
    else
      return do_var_expr(pctx, string, XPATH_NODE_ATTR);
  } 
  else if(x_is_selchar(*string->p))
  {
    NODETYPE nt;
    if(*string->p=='/')
    {
      nt = XPATH_NODE_ROOT;
      ++(string->p);
    } 
    else
      nt = XPATH_NODE_CONTEXT;

    node1 = do_select_expr(pctx, string);
    if(node1->type == XPATH_NODE_SELECT||node1->type == XPATH_NODE_DESCENDANT)
      node1->children = xml_new_node(pctx,NULL, nt); // case for root select

    return node1;
  }

  error("do_node2_expr:: malformed xpath expr at %s in %s",string->p,string->value);
  return NULL;
}


static
XMLNODE *do_node_expr(TRANSFORM_CONTEXT *pctx, XPATH_STRING *string)
{
  trace("do_node_expr:: expression: %s", string->p);
  XMLNODE *node1, *node2, *node3;
  
  node1 = do_node2_expr(pctx,string);
  for(;;) 
  {
    skip_ws(string);
    if(*string->p=='/') //continuing select
    {
      ++ (string->p);
      node2 = do_select_expr(pctx,string);
      node2->children = node1;
      node1 = node2;
    } 
    else if(*string->p=='[') // filter selection
    {
      ++ (string->p);
      node2 = do_or_expr(pctx,string);
      skip_ws(string);

      if(*string->p==']')
        ++(string->p);
      else
        error("do_node_expr:: unterminated select expr at %s in %s",string->p,string->value);

      node3                 = xml_new_node(pctx,NULL,XPATH_NODE_FILTER);
      node3->children       = node1; // previous select
      node3->children->next = node2; // filter argument
      node1                 = node3;
    } 
    else
      return node1;
  }
}


static
XMLNODE *do_union_expr(TRANSFORM_CONTEXT *pctx, XPATH_STRING *string)
{
  trace("do_union_expr:: expression: %s", string->p);
  XMLNODE *node1;
  XMLNODE *ornode;

  skip_ws(string);
  node1 = do_node_expr(pctx, string);
  skip_ws(string);

  if(*string->p=='|')
  {
    ++ (string->p);
    ornode = xml_new_node(pctx,NULL,XPATH_NODE_UNION);
    ornode->children = node1;
    skip_ws(string);
  } 
  else 
    return node1;

  do {
    node1->next = do_node_expr(pctx,string);
    node1=node1->next;
    skip_ws(string);
  } while (node1 && match(&(string->p), "|"));

  return ornode;
}


static
XMLNODE *do_not_expr(TRANSFORM_CONTEXT *pctx, XPATH_STRING *string)
{
  trace("do_not_expr:: expression: %s", string->p);
  XMLNODE *ornode;
  char    *old;

  skip_ws(string);
  old = string->p;

  if (match(&(string->p),"not"))
  {
    if(*string->p=='(' || x_is_ws(*string->p))
    {
      ornode = xml_new_node(pctx,NULL,XPATH_NODE_NOT);
      skip_ws(string);
      ornode->children = do_not_expr(pctx,string);
      return ornode;
    } 
    else
      string->p = old;
  }

  return do_union_expr(pctx,string);
}


static
XMLNODE *do_mul_expr(TRANSFORM_CONTEXT *pctx, XPATH_STRING *string)
{
  trace("do_mul_expr:: expression: %s", string->p);
  XMLNODE *node1;
  XMLNODE *ornode;
  node1 = do_not_expr(pctx,string);
  for(;;) {
    skip_ws(string);
    if(*string->p=='*') {
      ornode = xml_new_node(pctx,NULL,XPATH_NODE_MUL);
      ornode->children = node1;
      ++(string->p);
      skip_ws(string);
    } else if(match(&(string->p),"div ")) {
      ornode = xml_new_node(pctx,NULL,XPATH_NODE_DIV);
      ornode->children = node1;
      skip_ws(string);
    } else if(match(&(string->p),"mod ")) {
      ornode = xml_new_node(pctx,NULL,XPATH_NODE_MOD);
      ornode->children = node1;
      skip_ws(string);
    } else return node1;
    node1->next = do_not_expr(pctx,string);
    node1 = ornode;
  }
}


static
XMLNODE *do_add_expr(TRANSFORM_CONTEXT *pctx, XPATH_STRING *string)
{
  trace("do_add_expr:: expression: %s", string->p);
  XMLNODE *node1;
  XMLNODE *ornode;
  node1 = do_mul_expr(pctx,string);
  for(;;) {
    skip_ws(string);
    if(*string->p=='+') {
      ornode = xml_new_node(pctx,NULL,XPATH_NODE_ADD);
      ornode->children = node1;
      ++(string->p);
      skip_ws(string);
    } else if(*string->p=='-') {
      ornode = xml_new_node(pctx,NULL,XPATH_NODE_SUB);
      ornode->children = node1;
      ++(string->p);
      skip_ws(string);
    } else return node1;
    node1->next = do_mul_expr(pctx,string);
    node1 = ornode;
  }
}


static
XMLNODE *do_rel_expr(TRANSFORM_CONTEXT *pctx, XPATH_STRING *string)
{
  trace("do_rel_expr:: expression: %s", string->p);
  XMLNODE *node1;
  XMLNODE *ornode;
  node1 = do_add_expr(pctx,string);
  for(;;) {
    skip_ws(string);
    if(match(&(string->p),"<="))
    {
      ornode = xml_new_node(pctx,NULL,XPATH_NODE_LE);
      ornode->children = node1;
      skip_ws(string);
    } 
    else if(match(&(string->p),">="))
    {
      ornode = xml_new_node(pctx,NULL,XPATH_NODE_GE);
      ornode->children = node1;
      skip_ws(string);
    } 
    else if(match(&(string->p),"<"))
    {
      ornode = xml_new_node(pctx,NULL,XPATH_NODE_LT);
      ornode->children = node1;
      skip_ws(string);
    }
    else if(match(&(string->p),">"))
    {
      ornode = xml_new_node(pctx,NULL,XPATH_NODE_GT);
      ornode->children = node1;
      skip_ws(string);
    } 
    else 
      return node1;

    node1->next = do_add_expr(pctx,string);
    node1 = ornode;
  }
}

static
XMLNODE *do_eq_expr(TRANSFORM_CONTEXT *pctx, XPATH_STRING *string)
{
  trace("do_eq_expr:: expression: %s", string->p);
  XMLNODE *node1;
  XMLNODE *ornode;

  node1 = do_rel_expr(pctx, string);

  for(;;) 
  {
    skip_ws(string);

    if(*string->p=='=')
    {
      ornode = xml_new_node(pctx,NULL,XPATH_NODE_EQ);
      ornode->children = node1;
      ++(string->p);
      skip_ws(string);
    } 
    else if((string->p)[0]=='!' && (string->p)[1]=='=')
    {
      ornode = xml_new_node(pctx,NULL,XPATH_NODE_NE);
      ornode->children = node1;
      (string->p) += 2;
      skip_ws(string);
    } 
    else 
      return node1;

    node1->next = do_rel_expr(pctx,string);
    node1=ornode;
  }
}


static
XMLNODE *do_and_expr(TRANSFORM_CONTEXT *pctx, XPATH_STRING *string)
{
  trace("do_and_expr:: expression: %s", string->p);
  XMLNODE *node1;
  XMLNODE *ornode;

  node1 = do_eq_expr(pctx,string);
  skip_ws(string);

  if(match(&(string->p), "and "))
  {
    ornode = xml_new_node(pctx,NULL,XPATH_NODE_AND);
    ornode->children = node1;
    skip_ws(string);
  } 
  else 
    return node1;

  do {
    node1->next = do_eq_expr(pctx,string);
    node1=node1->next;
    skip_ws(string);
  } while(node1 && match(&(string->p), "and "));
  
  return ornode;
}


static
XMLNODE *do_or_expr(TRANSFORM_CONTEXT *pctx, XPATH_STRING *string)
{
  trace("do_or_expr:: expression: %s", string->p);
  XMLNODE *node1;
  XMLNODE *ornode;

  skip_ws(string);
  node1 = do_and_expr(pctx,string);
  skip_ws(string);

  if(match(&(string->p), "or "))
  {
    ornode = xml_new_node(pctx,NULL,XPATH_NODE_OR);
    ornode->children = node1;
    skip_ws(string);
  } 
  else 
    return node1;

  do {
    node1->next = do_and_expr(pctx,string);
    node1=node1->next;
    skip_ws(string);
  } while(node1 && match(&(string->p), "or "));

  return ornode;
}


XMLNODE *xpath_compile(TRANSFORM_CONTEXT *pctx, char *expr)
{
  XPATH_STRING string;
  string.value = expr;
  string.p = expr;

  memory_allocator_activate_parent(1);
  XMLNODE *result = do_or_expr(pctx, &string);
  memory_allocator_activate_parent(0);

  return result;
}
