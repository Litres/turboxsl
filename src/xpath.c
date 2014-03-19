
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
#include <unistd.h>
#include <math.h>

#include "ltr_xsl.h"

static XMLNODE *do_or_expr(TRANSFORM_CONTEXT *pctx, char **eptr);
void xpath_execute_scalar(TRANSFORM_CONTEXT *pctx, XMLNODE *locals, XMLNODE *etree, XMLNODE *current, RVALUE *res);


XMLNODE *xpath_in_selection(XMLNODE *sel, char *name)
{
  for(;sel;sel=sel->next) {
    if(sel->name == name) {
      return sel;
    }
  }
  return NULL;
}

void xpath_free_compiled(TRANSFORM_CONTEXT *pctx)
{
  unsigned i;
  for(i=0;i<pctx->n_exprs;++i) {
    xml_free_node(pctx,pctx->compiled[i].comp);
  }
  free(pctx->compiled);
}

XMLNODE *xpath_find_expr(TRANSFORM_CONTEXT *pctx, char *expr)
{
  XMLNODE *etree = NULL;
  char *e = hash(expr,-1,0);
  if(e==NULL)
    return NULL;
  unsigned i;
  for(i=0;i<pctx->n_exprs;++i) {
    if(pctx->compiled[i].expr==e) {
      etree = pctx->compiled[i].comp;
      break;
    }
  }
  if(etree==NULL) {
    etree = xpath_compile(pctx, e);
    if(pctx->n_exprs>=pctx->m_exprs) {
      pctx->m_exprs += 100;
      pctx->compiled = realloc(pctx->compiled, pctx->m_exprs*sizeof(EXPTAB));
    }
    if(etree) {
      pctx->compiled[pctx->n_exprs].expr = e;
      pctx->compiled[pctx->n_exprs].comp = etree;
      ++ pctx->n_exprs;
    }
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

char *node2string(XMLNODE *node)
{
  XMLSTRING str;

  if(node==NULL)
    return NULL;

  if(node->type == TEXT_NODE)
    return xml_strdup(node->content);

  str = xmls_new(100);
  add_node_str(str, node);

  return xmls_detach(str); //XXX leak!
}


XMLNODE *add_to_selection(XMLNODE *prev, XMLNODE *src, int *position)
{
  if(src==NULL)
    return prev;

  XMLNODE *dst = xml_new_node(NULL, NULL, src->type); //src->name already hashed, just copy pointer
  dst->name = src->name;
  dst->content = src->content;
  dst->attributes = src->attributes;
  dst->children = src->children;
  dst->parent = src->parent;
  dst->order = src->order;
  dst->position = ++(*position);
  if(prev) {
    dst->prev = prev;
    prev->next = dst;
  }
  return dst;
}

XMLNODE *xml_add_siblings(XMLNODE *prev, XMLNODE *src, int *position, XMLNODE **psel) // NOTE: does not copy nodes, used for selection unions only!
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

XMLNODE *xpath_select_nodes(TRANSFORM_CONTEXT *pctx, XMLNODE *current, char *name)
{
XMLNODE *r, *iter;
XMLNODE *set = NULL;
int pos;

  if(!current)
    return NULL;
  r = NULL;
  for(;current;current=current->next) {
    pos = 0; // restart numbering children for each node in source nodeset
    for(iter=current->children;iter;iter=iter->next) {
      if(iter->type==ELEMENT_NODE && (!name || iter->name==name)) {
        r = add_to_selection(r, iter, &pos);
        if(!set) {
          set = r;
        }
      }
    }
  }
  return set;
}

XMLNODE *xpath_copy_nodeset(XMLNODE *set)
{
  XMLNODE *node = NULL;
  XMLNODE *tmp = NULL;
  int pos = 0;
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
  int pos = 0;
  tmp = NULL;
  
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
  } else { // execute check expr for each node in nodeset, if true - add to new set
    for(;nodeset;nodeset=nodeset->next) {
      xpath_execute_scalar(pctx, locals, expr, nodeset, &rv);
      if(rval2bool(&rv)) {
        tmp = add_to_selection(tmp,nodeset,&pos);
        if(!node) {
          node = tmp;
        }
      }
    }
  }
  return node;    
}

static
XMLNODE *add_all_children(XMLNODE *tmp, XMLNODE *node, int *pos, XMLNODE **head)
{
int t;

  for(;node;node=node->next) {
    if(node->type == EMPTY_NODE) {
      tmp = add_all_children(tmp,node->children,pos,head);
      continue;
    }
    tmp = add_to_selection(tmp,node,pos);
    if(!(*head))
      *head = tmp;
    if(node->children)
      tmp = add_all_children(tmp,node->children,pos,head);
  }
  return tmp;
}

XMLNODE *xpath_get_descendants(XMLNODE *nodeset)
{
  XMLNODE *tmp = NULL;
  XMLNODE *head = xml_new_node(NULL, NULL, EMPTY_NODE);
  unsigned pos = 0;
  for(;nodeset;nodeset=nodeset->next) {
    tmp = add_all_children(tmp,nodeset->children,&pos,&(head->children));
  }
  head->flags|=0x8000; // XXX
  return head;
}

XMLNODE *xpath_get_attrs(XMLNODE *nodeset)
{
  XMLNODE *tmp = NULL;
  XMLNODE *head = NULL;
  XMLNODE *attr;
  unsigned pos = 0;

  for(;nodeset;nodeset=nodeset->next) {
    for(attr=nodeset->attributes;attr;attr=attr->next) {
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

  if(selection==NULL||selection->next==NULL)  //do not attempt to sort single-element or empty nodesets;
    return selection;
  newsel = selection;
  for(n=0;selection;selection=selection->next) { //first, count selection length and allocate tables
    ++n;
  }
  if(pctx->sort_size<n) {
    pctx->sort_size = n*2;
    pctx->sort_keys = (char **)realloc(pctx->sort_keys, pctx->sort_size*sizeof(char *));
    pctx->sort_nodes = (XMLNODE **)realloc(pctx->sort_nodes, pctx->sort_size*sizeof(XMLNODE *));
  }
  selection = newsel;
  for(i=0;i<n;++i) {
    pctx->sort_nodes[i] = selection;
    pctx->sort_keys[i] = sort->compiled?xpath_eval_string(pctx,locals,selection,sort->compiled):node2string(selection);
    selection = selection->next;
  }

  for(again=1;again;) { // do sorting
    again = 0;
    for(i=1;i<n;++i) {
      if(xml_strcasecmp(pctx->sort_keys[i-1],pctx->sort_keys[i])>0) {
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
  pctx->sort_nodes[0]->prev=NULL;
  for(i=1;i<n;++i) {
    pctx->sort_nodes[i]->prev = pctx->sort_nodes[i-1];
    pctx->sort_nodes[i-1]->next = pctx->sort_nodes[i];
    free(pctx->sort_keys[i]);
  }
  pctx->sort_nodes[n-1]->next = NULL;
  newsel = pctx->sort_nodes[0];
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
    res->type = VAL_NULL;
    return;
  }

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

    case XPATH_NODE_DESCENDANTS:
      xpath_execute_scalar(pctx,locals,etree->children,current,&rv); // selection
      res->type = VAL_NODESET;
      if(rv.type == VAL_NODESET) {
        selection = xpath_get_descendants(rv.v.nodeset);
        res->v.nodeset = selection;
      } else {  // can not select from non-nodeset
        res->v.nodeset = NULL;
      }
      rval_free(&rv);
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

    case XPATH_NODE_PARENT:
      if(etree->children) {
        xpath_execute_scalar(pctx,locals,etree->children,current,&rv); // XXX
        selection = add_to_selection(NULL,rv.v.nodeset->parent,&pos);
        res->type = VAL_NODESET;
        res->v.nodeset = selection;
      } else {
        selection = add_to_selection(NULL,current->parent,&pos);
        res->type = VAL_NODESET;
        res->v.nodeset = selection;
      }
      return;

    case XPATH_NODE_SELF:
      if(etree->children) {
        xpath_execute_scalar(pctx,locals,etree->children,current,res); // selection
      } else {
        selection = add_to_selection(NULL,current,&pos);
        res->type = VAL_NODESET;
        res->v.nodeset = selection;
      }
      return;

    case XPATH_NODE_CONTEXT:
      selection = add_to_selection(NULL,current,&pos);
      res->type = VAL_NODESET;
      res->v.nodeset = selection;
      return;

    case XPATH_NODE_FILTER:
      xpath_execute_scalar(pctx,locals,etree->children,current,&rv); // selection
      res->type = VAL_NODESET;
      if(rv.type == VAL_NODESET) {
        selection = xpath_filter(pctx,locals,rv.v.nodeset,etree->children->next);
        res->v.nodeset = selection;
      } else if(rv.type == VAL_STRING) {  // special case for @attr[1]
        res->type = VAL_STRING;
        res->v.string = rv.v.string;
        rv.type = VAL_NULL;
      } else {  // can not select from non-nodeset
        res->v.nodeset = NULL;
      }
      rval_free(&rv);
      return;

    case XPATH_NODE_SELECT:
      xpath_execute_scalar(pctx,locals,etree->children,current,&rv);
      res->type = VAL_NODESET;
      if(rv.type == VAL_NODESET) {
        selection = xpath_select_nodes(pctx,rv.v.nodeset,etree->name);
        res->v.nodeset = selection;
      } else {  // can not select from non-nodeset
        res->v.nodeset = NULL;
      }
      rval_free(&rv);
      return;

    case XPATH_NODE_ALL:
      xpath_execute_scalar(pctx,locals,etree->children,current,&rv);
      res->type = VAL_NODESET;
      if(rv.type == VAL_NODESET) {
        selection = xpath_select_nodes(pctx,rv.v.nodeset,NULL);
        res->v.nodeset = selection;
      } else {  // can not select from non-nodeset
        res->v.nodeset = NULL;
      }
      rval_free(&rv);
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
      res->type=VAL_BOOL;
      if(rval_equal(&rv, &rv1, 1)) {
        res->v.integer=1;
      } else {
      	res->v.integer=0;
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
      res->type = VAL_NUMBER;
      res->v.number = (double)((long)l % (long)r);
      return;

    case XPATH_NODE_NOT:
      xpath_execute_scalar(pctx,locals,etree->children,current,&rv);
      res->type=VAL_BOOL;
      res->v.integer=rval2bool(&rv)?0:1;
      return;

    case TEXT_NODE:
      res->type=VAL_STRING;
      res->v.string = strdup(etree->content);
      return;

    case XPATH_NODE_CALL:
      res->type=VAL_NULL;
      xpath_call_dispatcher(pctx,locals,etree->name,etree->children,current,res);
      return;

    case XPATH_NODE_ATTR:
      rv.type = VAL_NULL;
      if(etree->children) {
        xpath_execute_scalar(pctx,locals,etree->children,current,&rv);
        if(rv.type == VAL_NODESET)
          current = rv.v.nodeset;
        else
          current = NULL;
      }
      name=current?xml_get_attr(current,etree->name):NULL;
      if(name) {
        res->type = VAL_STRING;
        res->v.string = strdup(name);
      } else {
        res->type = VAL_NULL;
      }
      rval_free(&rv);
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
    xpath_execute_scalar(pctx,locals,etree,current,&rval);
    return rval2bool(&rval);
  }
  return 0;
}

char *xpath_eval_string(TRANSFORM_CONTEXT *pctx, XMLNODE *locals, XMLNODE *current, XMLNODE *etree)
{
  RVALUE rval;
  rval_init(&rval);
  char *s;

  if(etree) {
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
    xpath_execute_scalar(pctx,locals,etree,current,res);
  }
}

XMLNODE *xpath_eval_selection(TRANSFORM_CONTEXT *pctx, XMLNODE *locals, XMLNODE *current, XMLNODE *etree)
{
  RVALUE rval;
  rval_init(&rval);

  if(etree) {
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
  if(etree) {
    xpath_execute_scalar(pctx,locals,etree,current,this);
  }
}

/*
 * nodeset_free - frees nodeset if non-empty;
 *
 */

void xpath_free_selection(TRANSFORM_CONTEXT *pctx, XMLNODE *sel)
{
  XMLNODE *n;
  while(sel) {
    n = sel->next;
    nfree(pctx,sel);
    sel=n;
  }
}


/*
 * XPath expression compilation
 *
 */

static void skip_ws(char **eptr)
{
  char c;
  for(;;) {
    c = **eptr;
    if(!c)
      return;
    if(!x_is_ws(c))
      return;
    (*eptr)++;
  }
}

int match(char **eptr, char *str)
{
  char *pos = *eptr;
  char c;
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
XMLNODE *do_var_expr(TRANSFORM_CONTEXT *pctx, char **eptr, NODETYPE t)
{
  XMLNODE *ret;
  char *p, *e;

  p = (*eptr);
  for(e=p+1;*e && x_is_namechar(*e);++e)
    ;
  ret = xml_new_node(pctx, NULL, t);
  ret->name=hash(p,e-p,0);
  *eptr=e;
  skip_ws(eptr);
  return ret;
}


static
XMLNODE *do_select_expr(TRANSFORM_CONTEXT *pctx, char **eptr)
{
  XMLNODE *node1, *node2;
  char *p, *e;
  skip_ws(eptr);
  if(**eptr=='*') {
    ++(*eptr);
    return xml_new_node(pctx,NULL,XPATH_NODE_ALL);
  } else if(**eptr=='.') {
    ++(*eptr);
    if(**eptr=='.') {
      ++(*eptr);
      return xml_new_node(pctx,NULL,XPATH_NODE_PARENT);
    } else {
      return xml_new_node(pctx,NULL,XPATH_NODE_SELF);
    }
  } else if(**eptr=='/') {
    return xml_new_node(pctx,NULL,XPATH_NODE_DESCENDANTS);
  }

  p = (*eptr);
  if(*p=='@') {
    ++p;
    if(*p=='*') {
      ++p;
      *eptr = p;
      return xml_new_node(pctx,NULL,XPATH_NODE_ATTR_ALL);
    } else node1 = xml_new_node(pctx,NULL,XPATH_NODE_ATTR);
  } else {
    node1 = xml_new_node(pctx,NULL,XPATH_NODE_SELECT);
    if(!match(eptr,"child::")) { //skip default child prefix
      if(match(eptr,"attribute::")) {
        if(**eptr=='*') {
          ++(*eptr);
          node1->type = XPATH_NODE_ATTR_ALL;
          return node1;
        } else {
          node1->type = XPATH_NODE_ATTR;
        }
      }
    }
    p = *eptr;
  }

  for(e=p+1;*e && x_is_namechar(*e);++e)
      ;
  node1->name = hash(p,e-p,0);
  *eptr=e;

  skip_ws(eptr);
  if(**eptr=='(') { // call function
    ++(*eptr);
    node1->type = XPATH_NODE_CALL;
    for(;;) {
      skip_ws(eptr);
      if(**eptr==')') {
        ++ (*eptr);
        break;
      }
      node2 = do_or_expr(pctx,eptr);
      xml_add_child(pctx,node1,node2);
      skip_ws(eptr);
      if(**eptr==',')
        ++(*eptr);
    }
  }

  return node1;
}

char *errexp;

static
XMLNODE *do_node2_expr(TRANSFORM_CONTEXT *pctx, char **eptr)
{
  XMLNODE *node1, *node2;
  skip_ws(eptr);
  if(**eptr=='\'') {
    char *p, *e;
    p = ++(*eptr);
    node1 = xml_new_node(pctx,NULL,TEXT_NODE);
    for(e=p;*e && *e != '\'';++e)
        ;
    node1->content = malloc((e-p)+2);
    memcpy(node1->content,p,e-p);
    node1->content[e-p]=0;
    if(*e)++e;
    *eptr = e;
    skip_ws(eptr);
    return node1;
  } else if(**eptr=='(') {
    ++(*eptr);
    node1 = do_or_expr(pctx, eptr);
    skip_ws(eptr);
    if(**eptr==')') {
      ++(*eptr);
      skip_ws(eptr);
    }
    return node1;
  } else if (**eptr >= '0' && **eptr <= '9') {
    char *p, *e;
    int f_fl = 0;
    p = (*eptr);
    node1 = xml_new_node(pctx,NULL,TEXT_NODE);
    for(e=p+1;*e>='0' && *e <= '9';++e)
        ;
    if(*e=='.') {
      ++e;
      f_fl = 1;
    }
    for(;*e>='0' && *e <= '9';++e)
        ;
    if(f_fl) {
      node1->content = malloc(e-p+2);
      memcpy(node1->content,p,e-p);
      node1->content[e-p]=0;
    } else {
      char c = *e;
      *e = 0;
      node1->type = XPATH_NODE_INT;
      node1->extra.type = VAL_INT;
      node1->extra.v.integer = atol(p);
      *e = c;
    }
    *eptr = e;
    skip_ws(eptr);
    return node1;
  } else if (**eptr == '$') {
    ++(*eptr);
    return do_var_expr(pctx,eptr,XPATH_NODE_VAR);
  } else if (**eptr == '@') {
    ++(*eptr);
    if(**eptr=='*') {
      ++(*eptr);
      return xml_new_node(pctx,NULL,XPATH_NODE_ATTR_ALL);
    } else {
      return do_var_expr(pctx,eptr,XPATH_NODE_ATTR);
    }
  } else if(x_is_selchar(**eptr)) {
    NODETYPE nt;
    if(**eptr=='/') {
      nt = XPATH_NODE_ROOT;
      ++(*eptr);
    } else {
      nt = XPATH_NODE_CONTEXT;
    }
    node1 = do_select_expr(pctx, eptr);
    if(node1->type == XPATH_NODE_SELECT||node1->type == XPATH_NODE_DESCENDANTS) {
      node1->children = xml_new_node(pctx,NULL, nt); // case for root select
    }
    return node1;
  }
  fprintf(stderr,"\nmalformed xpath expr at %s in %s\n",*eptr,errexp);
  return NULL;
}

static
XMLNODE *do_node_expr(TRANSFORM_CONTEXT *pctx, char **eptr)
{
  XMLNODE *node1, *node2, *node3;
  
  node1 = do_node2_expr(pctx,eptr);
  for(;;) {
    skip_ws(eptr);
    if(**eptr=='/') {  //continuing select
      ++ (*eptr);
      node2 = do_select_expr(pctx,eptr);
      node2->children = node1;
      node1 = node2;
    } else if(**eptr=='[') { // filter selection
      ++ (*eptr);
      node2 = do_or_expr(pctx,eptr);
      skip_ws(eptr);
      if(**eptr==']')
        ++(*eptr);
      else {
        fprintf(stderr,"unterminated select expr at %s in %s\n",*eptr,errexp);
      }
      node3 = xml_new_node(pctx,NULL,XPATH_NODE_FILTER);
      node3->children = node1; // previous select
      node3->children->next = node2;
      node1 = node3;
    } else {
      return node1;
    }
  }
}

static
XMLNODE *do_union_expr(TRANSFORM_CONTEXT *pctx, char **eptr)
{
  XMLNODE *node1;
  XMLNODE *ornode;
  skip_ws(eptr);
  node1 = do_node_expr(pctx,eptr);
  skip_ws(eptr);
  if(**eptr=='|') {
    ++ (*eptr);
    ornode = xml_new_node(pctx,NULL,XPATH_NODE_UNION);
    ornode->children = node1;
    skip_ws(eptr);
  } else return node1;
  do {
    node1->next = do_node_expr(pctx,eptr);
    node1=node1->next;
    skip_ws(eptr);
  } while(node1 && match(eptr, "|"));
  return ornode;
}

static
XMLNODE *do_not_expr(TRANSFORM_CONTEXT *pctx, char **eptr)
{
  XMLNODE *ornode;
  char *old;
  skip_ws(eptr);
  old = *eptr;
  if(match(eptr,"not")) {
    if(**eptr=='(' || x_is_ws(**eptr)) {
      ornode = xml_new_node(pctx,NULL,XPATH_NODE_NOT);
      skip_ws(eptr);
      ornode->children = do_not_expr(pctx,eptr);
      return ornode;
    } else {
      *eptr = old;
    }
  }
  return do_union_expr(pctx,eptr);
}

static
XMLNODE *do_mul_expr(TRANSFORM_CONTEXT *pctx, char **eptr)
{
  XMLNODE *node1;
  XMLNODE *ornode;
  node1 = do_not_expr(pctx,eptr);
  for(;;) {
    skip_ws(eptr);
    if(**eptr=='*') {
      ornode = xml_new_node(pctx,NULL,XPATH_NODE_MUL);
      ornode->children = node1;
      ++(*eptr);
      skip_ws(eptr);
    } else if(match(eptr,"div ")) {
      ornode = xml_new_node(pctx,NULL,XPATH_NODE_DIV);
      ornode->children = node1;
      skip_ws(eptr);
    } else if(match(eptr,"mod ")) {
      ornode = xml_new_node(pctx,NULL,XPATH_NODE_DIV);
      ornode->children = node1;
      skip_ws(eptr);
    } else return node1;
    node1->next = do_not_expr(pctx,eptr);
    node1 = ornode;
  }
}


static
XMLNODE *do_add_expr(TRANSFORM_CONTEXT *pctx, char **eptr)
{
  XMLNODE *node1;
  XMLNODE *ornode;
  node1 = do_mul_expr(pctx,eptr);
  for(;;) {
    skip_ws(eptr);
    if(**eptr=='+') {
      ornode = xml_new_node(pctx,NULL,XPATH_NODE_ADD);
      ornode->children = node1;
      ++(*eptr);
      skip_ws(eptr);
    } else if(**eptr=='-') {
      ornode = xml_new_node(pctx,NULL,XPATH_NODE_SUB);
      ornode->children = node1;
      ++(*eptr);
      skip_ws(eptr);
    } else return node1;
    node1->next = do_mul_expr(pctx,eptr);
    node1 = ornode;
  }
}


static
XMLNODE *do_rel_expr(TRANSFORM_CONTEXT *pctx, char **eptr)
{
  XMLNODE *node1;
  XMLNODE *ornode;
  node1 = do_add_expr(pctx,eptr);
  for(;;) {
    skip_ws(eptr);
    if(match(eptr,"<=")) {
      ornode = xml_new_node(pctx,NULL,XPATH_NODE_LE);
      ornode->children = node1;
      skip_ws(eptr);
    } else if(match(eptr,">=")) {
      ornode = xml_new_node(pctx,NULL,XPATH_NODE_GE);
      ornode->children = node1;
      skip_ws(eptr);
    } else if(match(eptr,"<")) {
      ornode = xml_new_node(pctx,NULL,XPATH_NODE_LT);
      ornode->children = node1;
      skip_ws(eptr);
    } else if(match(eptr,">")) {
      ornode = xml_new_node(pctx,NULL,XPATH_NODE_GT);
      ornode->children = node1;
      skip_ws(eptr);
    } else return node1;
    node1->next = do_add_expr(pctx,eptr);
    node1 = ornode;
  }
}

static
XMLNODE *do_eq_expr(TRANSFORM_CONTEXT *pctx, char **eptr)
{
  XMLNODE *node1;
  XMLNODE *ornode;
  node1 = do_rel_expr(pctx,eptr);
  for(;;) {
    skip_ws(eptr);
    if(**eptr=='=') {
      ornode = xml_new_node(pctx,NULL,XPATH_NODE_EQ);
      ornode->children = node1;
      ++(*eptr);
      skip_ws(eptr);
    } else if((*eptr)[0]=='!' && (*eptr)[1]=='=') {
      ornode = xml_new_node(pctx,NULL,XPATH_NODE_NE);
      ornode->children = node1;
      (*eptr) += 2;
      skip_ws(eptr);
    } else return node1;
    node1->next = do_rel_expr(pctx,eptr);
    node1=ornode;
  }
}

static
XMLNODE *do_and_expr(TRANSFORM_CONTEXT *pctx, char **eptr)
{
  XMLNODE *node1;
  XMLNODE *ornode;
  node1 = do_eq_expr(pctx,eptr);
  skip_ws(eptr);
  if(match(eptr, "and ")) {
    ornode = xml_new_node(pctx,NULL,XPATH_NODE_AND);
    ornode->children = node1;
    skip_ws(eptr);
  } else return node1;
  do {
    node1->next = do_eq_expr(pctx,eptr);
    node1=node1->next;
    skip_ws(eptr);
  } while(node1 && match(eptr, "and "));
  return ornode;
}

static
XMLNODE *do_or_expr(TRANSFORM_CONTEXT *pctx, char **eptr)
{
  XMLNODE *node1;
  XMLNODE *ornode;
  skip_ws(eptr);
  node1 = do_and_expr(pctx,eptr);
  skip_ws(eptr);
  if(match(eptr, "or ")) {
    ornode = xml_new_node(pctx,NULL,XPATH_NODE_OR);
    ornode->children = node1;
    skip_ws(eptr);
  } else return node1;
  do {
    node1->next = do_and_expr(pctx,eptr);
    node1=node1->next;
    skip_ws(eptr);
  } while(node1 && match(eptr, "or "));
  return ornode;
}


XMLNODE *xpath_compile(TRANSFORM_CONTEXT *pctx, char *expr)
{
  char *eptr = errexp = expr;
  XMLNODE *res;

  res = do_or_expr(pctx,&eptr);
  return res;
}

