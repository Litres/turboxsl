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
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
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
  switch(rv->type) {
    case VAL_STRING:
      if(rv->v.string)
        free(rv->v.string);
      break;
    case VAL_NODESET:
      if(rv->v.nodeset)
        xpath_free_selection(NULL, rv->v.nodeset);
      break;
  }
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
        else if(rv->v.string[0]=='0' && 0.0==strtod(rv->v.string,NULL))
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
  XMLNODE *node;

  switch(rv->type) {
    case VAL_NULL:
      return NULL;

    case VAL_INT:
      rv->type=VAL_NULL;
      sprintf(s,"%ld",rv->v.integer);
      return strdup(s);

    case VAL_BOOL:
      rv->type=VAL_NULL;
      return strdup(rv->v.integer?"true":"false");

    case VAL_NUMBER:
      rv->type=VAL_NULL;
      sprintf(s,"%g",rv->v.number);
      return strdup(s);

    case VAL_STRING:
      rv->type=VAL_NULL;
      return rv->v.string;

    case VAL_NODESET:   // TODO: take first in document order
      res = NULL;
      if (rv->v.nodeset) {
        // fprintf(stderr, "call node2string()\n");
        // print_nodeset(
        //   rv->v.nodeset->type == EMPTY_NODE
        //     ? rv->v.nodeset->children
        //       : rv->v.nodeset
        // );

        res = node2string(
          rv->v.nodeset->type == EMPTY_NODE
            ? rv->v.nodeset->children
              : rv->v.nodeset
        );

        // fprintf(stderr, "res = %s\n", res);
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
        free(rv->v.string);
      } else {
        r = 0.0;
      }
      return r;
    case VAL_NODESET:
      rv->type=VAL_NULL;
      if(rv->v.nodeset) {
        p = node2string(rv->v.nodeset->children?rv->v.nodeset->children:rv->v.nodeset);
        if(p)
          r = strtod(p,NULL);
        else
          r = nan("");
        free(p);
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

/**************************** == and != ***************************/

int rval_equal(RVALUE *left, RVALUE *right, unsigned eq)
{
  int rc;
  double ld,rd;

  if(left->type == VAL_NULL)
    return 0;
  if(right->type == VAL_NULL)
    return 0;

  if(left->type == VAL_BOOL || left->type == VAL_BOOL) {
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
        if(0==xml_strcmp(l,r)) {
          free(r);
          free(l);
          rval_free(left);
          rval_free(right);
          return eq;
        }
        free(r);
      }
      free(l);
    }
    return !eq;
  }

  if(left->type == VAL_NODESET) {
    XMLNODE *p;
    char *l, *r;
    r = rval2string(right);
    for(p=left->v.nodeset;p;p=p->next) {
      l = node2string(p);
      if(0==xml_strcmp(l,r)) {
        free(r);
        free(l);
        rval_free(left);
        return eq;
      }
      free(l);
    }
    free(r);
    rval_free(left);
    return !eq;
  } else if(right->type == VAL_NODESET) {
    XMLNODE *p;
    char *l, *r;
    l = rval2string(left);
    for(p=right->v.nodeset;p;p=p->next) {
      r = node2string(p);
      if(0==xml_strcmp(l,r)) {
        free(r);
        free(l);
        rval_free(right);
        return eq;
      }
      free(r);
    }
    free(l);
    rval_free(right);
    return !eq;
  }

  if(left->type == VAL_STRING && right->type == VAL_STRING) {
    char *l, *r;
    l = rval2string(left);
    r = rval2string(right);
    rc = xml_strcmp(l,r)?!eq:eq;
    free(l);
    free(r);
    return rc;
  }

  ld = rval2number(left);
  rd = rval2number(right);
  return ld==rd?eq:!eq;
}

