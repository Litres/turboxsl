/*
 *  TurboXSL XML+XSLT processing library
 *  Debug and error display functions
 *
 *
 *
 *
 *  $Id: utils.c 34151 2014-03-03 18:41:50Z evozn $
 *
**/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#include "ltr_xsl.h"

void print_name_rec(XMLNODE *ns)
{
  if(ns && ns->parent) {
    print_name_rec(ns->parent);
    fprintf(stderr,"%s/",ns->name);
  }
}

void print_nodeset(XMLNODE *ns)
{
  fprintf(stderr,"(");
  while(ns) {
    if(ns->type==TEXT_NODE) {
      fprintf(stderr,"'%s'",ns->content);
    } else {
      if(ns->type!=EMPTY_NODE) {
        print_name_rec(ns);
      } else {
        print_nodeset(ns->children);
      }
    }
    ns = ns->next;
    if(ns)fprintf(stderr,",");
  }
  fprintf(stderr,")\n");
}

void print_rval(RVALUE *rv)
{
  switch(rv->type) {
    case VAL_NULL:
      fprintf(stderr,"(null)\n");
      break;
    case VAL_BOOL:
    case VAL_INT:
      fprintf(stderr,"%ld\n",rv->v.integer);
      break;
    case VAL_NUMBER:
      fprintf(stderr,"%f\n",rv->v.number);
      break;
    case VAL_STRING:
      fprintf(stderr,"'%s'\n",rv->v.string);
      break;
    case VAL_NODESET:
      print_nodeset(rv->v.nodeset);
      break;
  }
}

