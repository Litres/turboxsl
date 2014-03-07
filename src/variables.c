/*
 *  TurboXSL XML+XSLT processing library
 *  XSLT/XPATH variables
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

#include "ltr_xsl.h"
#include "xslglobs.h"

void  free_variables(TRANSFORM_CONTEXT *pctx)
{
  unsigned i;
  for(i=0;i<pctx->var_pos;++i) {
    rval_free(&(pctx->vars[i].extra));
  }
  free(pctx->vars);
}

XSL_VARIABLE *create_variable(TRANSFORM_CONTEXT *pctx, char *name)
{
  unsigned i;

  if(pctx->var_max==0) {
    pctx->var_max = 200;
    pctx->var_pos = 0;
    pctx->vars=malloc(pctx->var_max*sizeof(XSL_VARIABLE));
  } else if(pctx->var_pos>=pctx->var_max) {
    pctx->var_max += 100;
    pctx->vars=realloc(pctx->vars, pctx->var_max*sizeof(XSL_VARIABLE));
  }
  name = hash(name,-1,0);
  for(i=0;i<pctx->var_pos;++i) {
    if(pctx->vars[i].name == name) {
      return &(pctx->vars[i]);
    }
  }
  pctx->vars[pctx->var_pos].name = name;
  pctx->vars[pctx->var_pos].extra.type = VAL_NULL;
  ++pctx->var_pos;
  return &(pctx->vars[i]);
}

void set_ctx_global_var(TRANSFORM_CONTEXT *pctx, char *name, char *content)
{
  XSL_VARIABLE *var;
  name = hash(name,-1,0);
  content = xml_strdup(content);
  var = create_variable(pctx, name);
  var->extra.type = VAL_STRING;
  var->extra.v.string = content;
}

void set_global_var(XSLTGLOBALDATA *pctx, char *name, char *content)
{
  unsigned i;

  name = hash(name,-1,0);
  content = strdup(content);
  if(pctx->var_max==0) {
    pctx->var_max = 200;
    pctx->var_pos = 0;
    pctx->vars=malloc(pctx->var_max*sizeof(XSL_VARIABLE));
  } else if(pctx->var_pos>=pctx->var_max) {
    pctx->var_max += 100;
    pctx->vars=realloc(pctx->vars, pctx->var_max*sizeof(XSL_VARIABLE));
  }
  name = hash(name,-1,0);
  for(i=0;i<pctx->var_pos;++i) {
    if(pctx->vars[i].name == name) {
      rval_free(&(pctx->vars[i].extra));
      pctx->vars[i].extra.v.string = content;
      pctx->vars[i].extra.type = VAL_STRING;
      return;
    }
  }
  pctx->vars[pctx->var_pos].name = name;
  pctx->vars[pctx->var_pos].extra.v.string = content;
  pctx->vars[i].extra.type = VAL_STRING;
  ++pctx->var_pos;
}

void precompile_variables(TRANSFORM_CONTEXT *pctx, XMLNODE *stylesheet, XMLNODE *doc)
{
  XMLNODE *n;
  char *vname;
  char *vsel;
  XMLNODE dummy;
  XSL_VARIABLE *var;
  dummy.attributes = NULL;

  for(;stylesheet;stylesheet=n) {
    n = stylesheet->next;
    if(stylesheet->name==xsl_var) {
      vname = hash(xml_get_attr(stylesheet,xsl_a_name),-1,0);
      vsel = xml_get_attr(stylesheet,xsl_a_select);
      var = create_variable(pctx, vname);
      if(!vsel) {
        vsel = xml_eval_string(pctx, &dummy, doc, stylesheet->children);
        var->extra.type = VAL_STRING;
        var->extra.v.string = vsel;
      } else {
        xpath_eval_node(pctx, &dummy, doc, vsel, &(var->extra));
      }
      xml_unlink_node(stylesheet);
      stylesheet->children = NULL;
      stylesheet->next = NULL;
      
    } else if(stylesheet->children && stylesheet->name != xsl_template) {
      precompile_variables(pctx, stylesheet->children, doc);
    }
  }
}

char *get_variable_str(TRANSFORM_CONTEXT *pctx, XMLNODE *vars, char *name)
{
  unsigned i;
  if(vars && vars->attributes) {  //first try locals
    for(vars=vars->attributes;vars;vars=vars->next) {
      if(name==vars->name) {
        return vars->extra.v.string;
      }
    }
  }
  for(i=0;i<pctx->var_pos;++i) { // then globals
    if(name==pctx->vars[i].name) {
      return pctx->vars[i].extra.v.string;
    }
  }

  return NULL;
}

void rval_copy(TRANSFORM_CONTEXT *pctx, RVALUE *dst, RVALUE *src)
{
  switch(dst->type = src->type) {
    case VAL_BOOL:
    case VAL_INT:
      dst->v.integer = src->v.integer;
      break;
    case VAL_NUMBER:
      dst->v.number = src->v.number;
      break;
    case VAL_STRING:
      dst->v.string = xml_strdup(src->v.string);
      break;
    case VAL_NODESET:
      dst->v.nodeset = xpath_nodeset_copy(pctx, src->v.nodeset);
      break;
    default:
      dst->v.string = NULL;   // not really needed, just in case
  }
}

void get_variable_rv(TRANSFORM_CONTEXT *pctx, XMLNODE *vars, char *name, RVALUE *rv)
{
  unsigned i;
  if(vars && vars->attributes) {  //first try locals
    for(vars=vars->attributes;vars;vars=vars->next) {
      if(name==vars->name) {
        rval_copy(pctx,rv,&vars->extra);
        return;
      }
    }
  }
  for(i=0;i<pctx->var_pos;++i) { // then globals
    if(name==pctx->vars[i].name) {
      rval_copy(pctx,rv,&pctx->vars[i].extra);
      return;
    }
  }
  rv->type = VAL_NULL;
}



/*
 * set local variable
 * called from <xsl:variable>
 */

void do_local_var(TRANSFORM_CONTEXT *pctx, XMLNODE *vars, XMLNODE *doc, XMLNODE *var)
{
  char *vname, *vsel;
  unsigned i;
  XMLNODE *tmp;

  vname = hash(xml_get_attr(var,xsl_a_name),-1,0); 
  vsel = xml_get_attr(var,xsl_a_select);

  for(tmp=vars->attributes;tmp;tmp=tmp->next) {
    if(vname==tmp->name) {
      free(tmp->content);
      tmp->content = NULL;
      break;
    }
  }
  if(!tmp) {
    tmp = xml_new_node(pctx, vname, ATTRIBUTE_NODE);
    tmp->next = vars->attributes;
    vars->attributes = tmp;
  }

  if(!vsel) {
     tmp->extra.v.nodeset = xml_new_node(pctx, NULL, EMPTY_NODE);
     tmp->extra.type = VAL_NODESET;
     apply_xslt_template(pctx, tmp->extra.v.nodeset, doc, var->children, NULL, vars);
  } else {
     xpath_eval_node(pctx, vars, doc, vsel, &(tmp->extra));
  }
}

/*
 * copy template param to local variable
 * called at <xsl:param>
 */

void add_local_var(TRANSFORM_CONTEXT *pctx, XMLNODE *vars, char *name, XMLNODE *params)
{
XMLNODE *tmp;

  for(;params;params=params->next) {
    if(name==params->name) {
      tmp = xml_new_node(pctx, name, ATTRIBUTE_NODE);
      tmp->next = vars->attributes;
      vars->attributes = tmp;
      tmp->extra = params->extra;
      params->extra.type = VAL_NULL;
      params->name = NULL;
      
      return;
    }
  }
}

char *xsl_get_global_key(TRANSFORM_CONTEXT *pctx, char *first, char *second)
{
  char fullname[200];
  int i;

  strcpy(fullname,"@");
  strcat(fullname,first);
  if(second) {
    strcat(fullname,"@");
    strcat(fullname,second);
  }

  for(i=0;i<pctx->var_pos;++i) {
    if(0==strcmp(fullname,pctx->vars[i].name)) {
      return xml_strdup(pctx->vars[i].extra.v.string);
    }
  }
  for(i=0;i<pctx->gctx->var_pos;++i) {
    if(0==strcmp(fullname,pctx->gctx->vars[i].name)) {
      return xml_strdup(pctx->gctx->vars[i].extra.v.string);
    }
  }
  return NULL;
}