/*
 *  TurboXSL XML+XSLT processing library
 *  RVALUE processing. part of XPATH compiler
 *
 *
 *  (c) Egor Voznessenski, voznyak@mail.ru
 *
 *  $Id$
 *
**/


#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include "ltr_xsl.h"


/******************* reset rvalue, NULLing is ok ***************/

void rval_init(RVALUE *rv)
{
  rv->type = VAL_NULL;
}


/******************* frees allocated storage ***************/

void rval_free(RVALUE *rv)
{
  rv->type = VAL_NULL;
}

/******************* convert to boolean, boolean() function ***************/

int rval2bool(RVALUE *rv)
{
  int res;
  switch(rv->type) {
    case VAL_NULL:
      return 0;
    case VAL_INT:
    case VAL_BOOL:
      rv->type=VAL_NULL;
      return (rv->v.integer)?1:0;
    case VAL_NUMBER:
      rv->type=VAL_NULL;
      return (rv->v.number==0)?0:1;
    case VAL_STRING:
      if(!rv->v.string)
        return 0;
      else {
        if(rv->v.string[0]==0)
          res = 0;
        else
          res = 1;
        rval_free(rv);
        return res;
      }
    case VAL_NODESET:
      if(!rv->v.nodeset)
        return 0;
      else {
        rval_free(rv);
        return 1;
      }
    default:
      return 0;
  }
}

/******************* convert to string, string() function ***************/

char *rval2string(RVALUE *rv) {
  char s[200];
  char *res;

  switch(rv->type) {
    case VAL_NULL:
      return NULL;

    case VAL_INT:
      rv->type=VAL_NULL;
      sprintf(s,"%ld",rv->v.integer);
      return xml_strdup(s);

    case VAL_BOOL:
      rv->type=VAL_NULL;
      return xml_strdup(rv->v.integer?"true":"false");

    case VAL_NUMBER:
      rv->type=VAL_NULL;
      sprintf(s,"%g",rv->v.number);
      return xml_strdup(s);

    case VAL_STRING:
      rv->type=VAL_NULL;
      return rv->v.string;

    case VAL_NODESET:
      res = NULL;
      if(rv->v.nodeset) {
        // TODO take first in document order
        XMLNODE *node = rv->v.nodeset;
        res = node->type == EMPTY_NODE ? nodes2string(node) : node2string(node);
      }
      rval_free(rv);
      return res;

    default:
      return NULL;
  }
}


/******************* convert to number, number() function ***************/

double rval2number(RVALUE *rv)
{
double r;
char *p,*n;

  switch(rv->type) {
    case VAL_NULL:
      return 0.0; //maybe nan?
    case VAL_BOOL:
      rv->type=VAL_NULL;
      return (double)(rv->v.integer?1:0);
    case VAL_INT:
      rv->type=VAL_NULL;
      return (double)(rv->v.integer);
    case VAL_NUMBER:
      rv->type=VAL_NULL;
      return (rv->v.number);
    case VAL_STRING:
      rv->type=VAL_NULL;
      if(rv->v.string) {
        for(p=rv->v.string;x_is_ws(*p);++p)
            ;
        n = p;
        if(*p=='-')++p;
        if(*p=='.'||(*p>='0'&&*p<='9'))
            r = strtod(n, NULL);
        else
            r = nan("");
      } else {
        r = 0.0;
      }
      return r;
    case VAL_NODESET:
      rv->type=VAL_NULL;
      if(rv->v.nodeset) {
        p = node2string(rv->v.nodeset->children?rv->v.nodeset->children:rv->v.nodeset);
        if(p) {
          RVALUE t;
          t.type = VAL_STRING;
          t.v.string = p;
          r = rval2number(&t);
        } else {
          r = nan("");
        }
      } else {
        r = nan("");
      }
      xpath_free_selection(NULL,rv->v.nodeset);
      return r;
    default:
      return 0.0;
  }
}

/**************************** < <= > >= ***************************/


int rval_compare(RVALUE *left, RVALUE *right)
{
  double ld,rd;

  if(left->type == VAL_NULL) {
    if(right->type == VAL_NULL)
      return 0;
    else
      return -1;
  } else if(right->type == VAL_NULL) {
    return 1;
  }

  ld = rval2number(left);
  rd = rval2number(right);
  if(ld < rd) {
    return -1;
  }
  else if(ld>rd) {
    return 1;
  }
  else return 0;
}

int nodeset_equal_common(RVALUE *value, RVALUE *nodeset, unsigned eq)
{
  XMLNODE *node = nodeset->v.nodeset;
  if (node->type == EMPTY_NODE) node = node->children;

  RVALUE t;
  for(XMLNODE *p = node; p; p = p->next) {
    t.type = VAL_NODESET;
    t.v.nodeset = p;

    if(value->type == VAL_NUMBER || value->type == VAL_INT) {
      double a = value->type == VAL_NUMBER ? value->v.number : value->v.integer;
      double b = rval2number(&t);
      if (eq ? a == b : a != b) return 1;
    }

    if(value->type == VAL_STRING) {
      char *string = rval2string(&t);
      if (eq ? xml_strcmp(value->v.string, string) == 0 : xml_strcmp(value->v.string, string) != 0) return 1;
    }
  }

  return 0;
}

/**************************** == and != ***************************/

int rval_equal(RVALUE *left, RVALUE *right, unsigned eq)
{
  trace("rval_equal:: a (%d) %s b (%d)", left->type, eq == 0 ? "!=" : "=", right->type);
  int rc;
  double ld,rd;

  if(left->type == VAL_NULL)
    return 0;
  if(right->type == VAL_NULL)
    return 0;

  if(left->type == VAL_BOOL || right->type == VAL_BOOL) {
    int lb,rb;
    lb = rval2bool(left);
    rb = rval2bool(right);
    return lb==rb?eq:!eq;
  }

  if(left->type == VAL_NODESET && right->type == VAL_NODESET) {
    XMLNODE *ln,*rn;
    char *l,*r;
    for(ln=left->v.nodeset;ln;ln=ln->next) {
      l=node2string(ln);
      for(rn=right->v.nodeset;rn;rn=rn->next) {
        r=node2string(rn);
        // depends on eq search for non equal or equal pair
        if(eq == 0 ? xml_strcmp(l,r) != 0 : xml_strcmp(l,r) == 0) {
          rval_free(left);
          rval_free(right);
          return 1;
        }
      }
    }
    return 0;
  }

  if(left->type == VAL_NODESET) {
    return nodeset_equal_common(right, left, eq);
  } else if(right->type == VAL_NODESET) {
    return nodeset_equal_common(left, right, eq);
  }

  if(left->type == VAL_STRING && right->type == VAL_STRING) {
    char *l, *r;
    l = rval2string(left);
    r = rval2string(right);
    rc = xml_strcmp(l,r)?!eq:eq;
    return rc;
  }

  ld = rval2number(left);
  rd = rval2number(right);
  return ld==rd?eq:!eq;
}

