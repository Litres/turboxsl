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

#include "ltr_xsl.h"
#include "xsl_elements.h"
#include "template_task.h"

void free_variables(TRANSFORM_CONTEXT *pctx)
{
  unsigned i;
  for(i=0;i<pctx->var_pos;++i) {
    rval_free(&(pctx->vars[i].extra));
  }
  pctx->var_max = 0;
  pctx->var_pos = 0;
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

  for(i=0;i<pctx->var_pos;++i) {
    if(xml_strcmp(pctx->vars[i].name, name) == 0) {
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
  XSL_VARIABLE *var = create_variable(pctx, xml_strdup(name));
  var->extra.type = VAL_STRING;
  var->extra.v.string = xml_strdup(content);
}

void set_global_var(XSLTGLOBALDATA *pctx, char *name, char *content)
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

  for(i=0;i<pctx->var_pos;++i) {
    if(xml_strcmp(pctx->vars[i].name, name) == 0) {
      rval_free(&(pctx->vars[i].extra));
      pctx->vars[i].extra.v.string = xml_strdup(content);
      pctx->vars[i].extra.type = VAL_STRING;
      return;
    }
  }
  pctx->vars[pctx->var_pos].name = xml_strdup(name);
  pctx->vars[pctx->var_pos].extra.v.string = xml_strdup(content);
  pctx->vars[i].extra.type = VAL_STRING;
  ++pctx->var_pos;
}

void precompile_variables(TRANSFORM_CONTEXT *pctx, XMLNODE *stylesheet, XMLNODE *doc)
{
  XMLNODE *n;
  XMLNODE dummy;
  XSL_VARIABLE *var;
  dummy.attributes = NULL;

  for(;stylesheet;stylesheet=n) {
    n = stylesheet->next;
    if(xmls_equals(stylesheet->name, xsl_var)) {
      XMLSTRING vname = xml_get_attr(stylesheet,xsl_a_name);
      XMLSTRING vsel = xml_get_attr(stylesheet,xsl_a_select);
      var = create_variable(pctx, vname->s);
      if(!vsel) {
        vsel = xml_eval_string(pctx, &dummy, doc, stylesheet->children);
        var->extra.type = VAL_STRING;
        var->extra.v.string = vsel->s;
      } else {
        xpath_eval_node(pctx, &dummy, doc, vsel, &(var->extra));
      }
    } else if(stylesheet->children && !xmls_equals(stylesheet->name, xsl_template)) {
      precompile_variables(pctx, stylesheet->children, doc);
    }
  }
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

void get_variable_rv(TRANSFORM_CONTEXT *pctx, XMLNODE *vars, XMLSTRING name, RVALUE *rv)
{
  unsigned i;
  if(vars && vars->attributes) {  //first try locals
    for(vars=vars->attributes;vars;vars=vars->next) {
      if(xmls_equals(name, vars->name)) {
        rval_copy(pctx,rv,&vars->extra);
        return;
      }
    }
  }
  for(i=0;i<pctx->var_pos;++i) { // then globals
    if(xml_strcmp(name->s, pctx->vars[i].name) == 0) {
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
  XMLNODE *tmp;

  XMLSTRING vname = xml_get_attr(var,xsl_a_name);
  XMLSTRING vsel = xml_get_attr(var,xsl_a_select);

  for(tmp=vars->attributes;tmp;tmp=tmp->next) {
    if(xmls_equals(vname, tmp->name)) {
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

    template_context *new_context = memory_allocator_new(sizeof(template_context));
    new_context->context = pctx;
    new_context->instruction = var->children;
    new_context->result = tmp->extra.v.nodeset;
    new_context->document_node = doc;
    new_context->local_variables = vars;
    new_context->task_mode = SINGLE;

    apply_xslt_template(new_context);
  }
  else {
    xpath_eval_node(pctx, vars, doc, vsel, &(tmp->extra));
  }
}

/*
 * copy template param to local variable
 * called at <xsl:param>
 */

void add_local_var(TRANSFORM_CONTEXT *pctx, XMLNODE *vars, XMLSTRING name, XMLNODE *params)
{
  XMLNODE *tmp;

  for (; params; params = params->next) {
    if (xmls_equals(name, params->name))
    {
      tmp              = xml_new_node(pctx, name, ATTRIBUTE_NODE);
      tmp->next        = vars->attributes;
      vars->attributes = tmp;

      rval_copy(pctx, &(tmp->extra), &(params->extra));

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
    if(0==xml_strcmp(fullname,pctx->vars[i].name)) {
      return xml_strdup(pctx->vars[i].extra.v.string);
    }
  }
  for(i=0;i<pctx->gctx->var_pos;++i) {
    if(0==xml_strcmp(fullname,pctx->gctx->vars[i].name)) {
      return xml_strdup(pctx->gctx->vars[i].extra.v.string);
    }
  }
  return NULL;
}

XMLNODE *copy_variables(TRANSFORM_CONTEXT *context, XMLNODE *variables)
{
  XMLNODE *result = xml_new_node(context, NULL, EMPTY_NODE);
  if(variables == NULL) return result;
  
  for(XMLNODE *variable = variables->attributes; variable; variable = variable->next) {
    XMLNODE *copy = xml_new_node(context, variable->name, ATTRIBUTE_NODE);
    copy->extra.type = variable->extra.type;
    if(variable->extra.type == VAL_BOOL || variable->extra.type == VAL_INT) {
      copy->extra.v.integer = variable->extra.v.integer;
    }

    if(variable->extra.type == VAL_NUMBER) {
      copy->extra.v.number = variable->extra.v.number;
    }

    if(variable->extra.type == VAL_STRING) {
      copy->extra.v.string = variable->extra.v.string;
    }

    if(variable->extra.type == VAL_NODESET) {
      copy->extra.v.nodeset = variable->extra.v.nodeset;
    }

    copy->next = result->attributes;
    result->attributes = copy;
  }
  return result;
}
