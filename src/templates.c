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

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#include "ltr_xsl.h"
#include "xslglobs.h"


static XMLNODE enode;
char *fn_text = NULL;
char *fn_node = NULL;

static int templtab_add(TRANSFORM_CONTEXT *pctx, XMLNODE * content, char *name)
{
unsigned r;

  if(pctx->templcnt>=pctx->templno) {
    if(fn_text==NULL) {
      xml_clear_node(pctx,&enode);
      fn_text = hash("text",-1,0);
      fn_node = hash("node",-1,0);
    }
    pctx->templno += 100;
    pctx->templtab = realloc(pctx->templtab, pctx->templno*sizeof(TEMPLATE));
  }

  r = pctx->templcnt++;
  pctx->templtab[r].name = hash(name,-1,0);
  pctx->templtab[r].content = content;
  pctx->templtab[r].match = NULL;
  pctx->templtab[r].mode = NULL;
  pctx->templtab[r].expr = NULL;
  pctx->templtab[r].priority = 0.0;
  pctx->templtab[r].matchtype = TMATCH_NONE;
  return r;
}

static unsigned add_templ_match(TRANSFORM_CONTEXT *pctx, XMLNODE *content, char *match, char *mode)
{
  unsigned r;
  char *m1;

  while(x_is_ws(*match))  // skip leading whitespace;
    ++match;
  for(r=strlen(match);r;--r) {  //trailing whitespace
    if(x_is_ws(match[r-1])) {
      match[r-1]=0;
    } else {
      break;
    }
  }
  if(!content)  // if template match is empty, return empty node on match, not NULL
    content = &enode;

  mode = hash(mode,-1,0);
  m1 = hash(match,-1,0);

  r = templtab_add(pctx, content, NULL);
  pctx->templtab[r].match = m1;
  pctx->templtab[r].mode = mode;
  if(match[0]=='*' && match[1]==0) {
    pctx->templtab[r].matchtype = TMATCH_ALWAYS;
  } else if (strchr(match,'/')||strchr(match,'[')||strstr(match,"::")) { // looks like select match
    if(strchr(match+1,'/')||strchr(match+1,'[')||strstr(match,"::")) { // XXX not checking if first char is [ -- but this will probably never happen
      pctx->templtab[r].matchtype = TMATCH_SELECT;
      pctx->templtab[r].expr = xpath_find_expr(pctx,m1);
    } else {
      pctx->templtab[r].match = hash(match+1,-1,0);
      pctx->templtab[r].matchtype = TMATCH_ROOT;
    }
  } else {
    pctx->templtab[r].matchtype = TMATCH_EXACT;
  }
  return r;
}

static void add_template(TRANSFORM_CONTEXT *pctx, XMLNODE * content, char *name, char *match, char *mode)
{
  char *pp;

  if(match) {
    while(match) {
      if(strchr(match,'[')) {
        add_templ_match(pctx,content, match, mode);
        match = NULL;
      } else {
        pp = strchr(match,'|');
        if(pp) {
          *pp = 0;
          add_templ_match(pctx,content, match, mode);
          *pp = '|';
          match=pp+1;
        } else {
          add_templ_match(pctx,content, match, mode);
          match = NULL;
        }
      }
    }
  } else {
    dict_add(pctx->named_templ, name, content);
  }
}

static int select_match(TRANSFORM_CONTEXT *pctx, XMLNODE *node, XMLNODE *templ)
{
  XMLNODE *t;
  
  if(templ==NULL) // XXX TODO is this corrrect?
    return 1;

  if(node==NULL)
    return 0;

  switch(templ->type) {
    case XPATH_NODE_ROOT:
      return (pctx->root_node == node);

    case XPATH_NODE_SELECT:
      if(node->name != templ->name)
        return 0;
      else return select_match(pctx, node->parent, templ->children);

    case XPATH_NODE_CONTEXT:
        return 1;

    case XPATH_NODE_SELF:
        return select_match(pctx, node, templ->children);

    case XPATH_NODE_FILTER:
        if(!select_match(pctx, node, templ->children))
          return 0;
        if(templ->children->next->type==XPATH_NODE_UNION || templ->children->next->type==XPATH_NODE_SELECT) {
          return select_match(pctx,node->children,templ->children->next);
        } else {
          return 1;
        }

    case XPATH_NODE_UNION:
        for(;node;node=node->next) {
          for(t=templ->children;t;t=t->next) {
            if(select_match(pctx,node,t))
              return 1;
          }
        }
        return 0;

    case XPATH_NODE_ALL:
        return select_match(pctx, node->parent, templ->children);

    case XPATH_NODE_CALL:
        if(templ->name==fn_text) {
          return node->type==TEXT_NODE;
        } else if(templ->name==fn_node) {
          return 1;
        } else return 0;

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

XMLNODE *find_template(TRANSFORM_CONTEXT *pctx, XMLNODE *node, char *mode) // name here is node name, so is already hashed
{
  XMLNODE *tmp = NULL;
  XMLNODE *selection;

  char *name = node->name;
  //fprintf(stderr, "find_template: name: %s\n", name);

  int i;
  for(i=0;i<pctx->templcnt;++i) {
    if(pctx->templtab[i].mode!=mode)
      continue;
    switch (pctx->templtab[i].matchtype) {
      case TMATCH_ROOT:
        if(pctx->templtab[i].match[0]==0 && node==pctx->root_node) {  // matches "/"
          return pctx->templtab[i].content;
        }
        if(node->parent!=pctx->root_node)
          continue;
        if(pctx->templtab[i].match[0]=='*') { // matches "/*"
          return pctx->templtab[i].content;
        }
    /*** or if it is a child of root, fall thru to exact name match ***/
      case TMATCH_EXACT:
        if(name == pctx->templtab[i].match) {
          return pctx->templtab[i].content;
        }
        break;
      case TMATCH_SELECT:
        if(select_match(pctx, node, pctx->templtab[i].expr)) {
          return pctx->templtab[i].content;
        }
        break;
    }
  }

  if(node!=pctx->root_node) {
    for(i=0;i<pctx->templcnt;++i) {
      if(pctx->templtab[i].mode==mode && pctx->templtab[i].matchtype==TMATCH_ALWAYS) {
        return pctx->templtab[i].content;
      }
    }
  }

  return NULL;
}


XMLNODE *template_byname(TRANSFORM_CONTEXT *pctx, char *name)
{
  int i;
  XMLNODE *r;

  if(name==NULL)
    return NULL;
  name = hash(name,-1,0);
  return dict_find(pctx->named_templ,name);
}

void  precompile_templates(TRANSFORM_CONTEXT *pctx, XMLNODE *node)
{
  for(;node;node=node->next) {
    if(node->type==ELEMENT_NODE && node->name==xsl_template) {
      char *name = hash(xml_get_attr(node,xsl_a_name),-1,0);
      char *match = xml_get_attr(node,xsl_a_match);
      char *mode = xml_get_attr(node,xsl_a_mode);
      add_template(pctx, node->children,name,match,mode);
    }
    if(node->children)
      precompile_templates(pctx, node->children);
  }
}