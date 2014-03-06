/*
 *  TurboXSL XML+XSLT processing library
 *  Template precompiler / match routines
 *
 *
 *
 *
 *  $Id: templates.c 34192 2014-03-04 16:05:40Z evozn $
 *
**/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#include "ltr_xsl.h"
#include "xslglobs.h"

typedef struct {
  char *name;
  char *match;
  char *mode;
  XMLNODE *expr;
  enum {TMATCH_NONE, TMATCH_ALWAYS, TMATCH_EXACT, TMATCH_START, TMATCH_RE, TMATCH_ROOT, TMATCH_SELECT} matchtype;
  XMLNODE *content;
  unsigned subtempl;
} TEMPLATE;

TEMPLATE *templtab = NULL;
unsigned templno = 0;
unsigned templcnt = 0;
static XMLNODE enode;
char *fn_text = NULL;
char *fn_node = NULL;

static int templtab_add(TRANSFORM_CONTEXT *pctx, XMLNODE * content, char *name)
{
unsigned r;

  if(templcnt>=templno) {
    if(fn_text==NULL) {
      xml_clear_node(pctx,&enode);
      fn_text = hash("text",-1,0);
      fn_node = hash("node",-1,0);
    }
    templno += 100;
    templtab = realloc(templtab, templno*sizeof(TEMPLATE));
  }
//fprintf(stderr,"add template %s match %s\n",name?name:"",match?match:"*");
  templtab[templcnt].name = hash(name,-1,0);
  templtab[templcnt].content = content;
  templtab[templcnt].match = NULL;
  templtab[templcnt].mode = NULL;
  templtab[templcnt].expr = NULL;
  templtab[templcnt].subtempl = 0;
  templtab[templcnt].matchtype = TMATCH_NONE;
  r = templcnt++;
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
// exact node name match
//fprintf(stderr,"add match %s:%s\n",match,mode);

  mode = hash(mode,-1,0);
  m1 = hash(match,-1,0);
//  for(r=0; r<templcnt; ++r) {
//    if(templtab[r].mode == mode && templtab[r].match == m1)
//      return 0;
//  }
  r = templtab_add(pctx, content, NULL);
  templtab[r].match = m1;
  templtab[r].mode = mode;
  if(match[0]=='*' && match[1]==0) {
    templtab[r].matchtype = TMATCH_ALWAYS;
  } else if (strchr(match,'/')||strchr(match,'[')||strstr(match,"::")) { // looks like select match
    if(strchr(match+1,'/')||strchr(match+1,'[')||strstr(match,"::")) { // XXX not checking if first char is [ -- but this will probably never happen
      templtab[r].matchtype = TMATCH_SELECT;
      templtab[r].expr = xpath_find_expr(pctx,m1);
    } else {
      templtab[r].match = hash(match+1,-1,0);
      templtab[r].matchtype = TMATCH_ROOT;
    }
  } else {
    templtab[r].matchtype = TMATCH_EXACT;
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
    templtab_add(pctx, content, name);
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

    default:
      return 0;
  }
}

XMLNODE *tmpl_match(TRANSFORM_CONTEXT *pctx,XMLNODE *node, char *mode) // name here is node name, so is already hashed
{
  XMLNODE *tmp = NULL;
  XMLNODE *selection;

  char *name = node->name;
  int i;
  for(i=0;i<templcnt;++i) {
    if(templtab[i].mode!=mode)
      continue;
//    if(templtab[i].matchtype==TMATCH_ALWAYS && node!=pctx->root_node) {
//      tmp = templtab[i].content?templtab[i].content:&enode;
//fprintf(stderr,"match %s -> %s:%s\n",name,templtab[i].match,mode);
//      return tmp;
//    }
    switch (templtab[i].matchtype) {
      case TMATCH_ROOT:
//fprintf(stderr,"match %s -> /%s:%s\n",name,templtab[i].match,mode);
        if(templtab[i].match[0]==0 && node==pctx->root_node) {
          tmp = templtab[i].content?templtab[i].content:&enode;
          return tmp;
        }
        if(node->parent!=pctx->root_node)
          continue;
        if(templtab[i].match[0]=='*') {
          tmp = templtab[i].content?templtab[i].content:&enode;
          return tmp;
        }
    /*** or if it is a child of root, fall thru to exact name match ***/
      case TMATCH_EXACT:
        if(name == templtab[i].match) {
          tmp = templtab[i].content?templtab[i].content:&enode;
//fprintf(stderr,"match %s -> %s:%s\n",name,templtab[i].match,mode);
          return tmp;
        }
        break;
      case TMATCH_SELECT:
        if(select_match(pctx, node, templtab[i].expr)) {
          tmp = templtab[i].content?templtab[i].content:&enode;
//fprintf(stderr,"match * -> %s:%s\n",templtab[i].match,mode);
          return tmp;
        }
        break;
    }
  }

  if(node!=pctx->root_node) {
    for(i=0;i<templcnt;++i) {
      if(templtab[i].mode==mode && templtab[i].matchtype==TMATCH_ALWAYS) {
        tmp = templtab[i].content?templtab[i].content:&enode;
//fprintf(stderr,"match %s -> %s:%s\n",name,templtab[i].match,mode);
        return tmp;
      }
    }
  }
//fprintf(stderr,"no match\n");
  return NULL;
}


XMLNODE *find_template(TRANSFORM_CONTEXT *pctx, XMLNODE *source, char *mode)
{
XMLNODE *match;

  match = tmpl_match(pctx, source, mode);
//fprintf(stderr,"templ %p for %s\n",match,source->name);
  if(match) {
    return match;
  }
  return NULL;
}

XMLNODE *template_byname(TRANSFORM_CONTEXT *pctx, char *name)
{
  int i;

  if(name==NULL)
    return NULL;
  for(i=0;i<templcnt;++i) {
    if(NULL == templtab[i].name)
      continue;
    if((name == templtab[i].name) || (!strcmp(name, templtab[i].name)))
      return templtab[i].content;
  }
  return NULL;
}

void  precompile_templates(TRANSFORM_CONTEXT *pctx, XMLNODE *node)
{
  for(;node;node=node->next) {
    if(node->type==ELEMENT_NODE && node->name==xsl_template) {
      char *name = xml_get_attr(node,xsl_a_name);
      char *match = xml_get_attr(node,xsl_a_match);
      char *mode = xml_get_attr(node,xsl_a_mode);
      add_template(pctx, node->children,name,match,mode);
    }
    if(node->children)
      precompile_templates(pctx, node->children);
  }
}