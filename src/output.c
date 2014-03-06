/*
 *  TurboXSL XML+XSLT processing library
 *  XML nodeset to text convertor,
 *  file output.
 *
 *
 *
 *  $Id: output.c 34247 2014-03-05 14:48:37Z evozn $
 *
**/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "ltr_xsl.h"
#include "xslglobs.h"


void add_quoted_str(XMLSTRING rtext, char *s)
{
  while(*s) {
    if(*s=='"') {
      xmls_add_str(rtext,"&quot;");
    } else if(*s=='<') {
      xmls_add_str(rtext,"&lt;");
    } else if(*s=='>') {
      xmls_add_str(rtext,"&gt;");
    } else if(*s=='&' && s[1]!='#') {
      xmls_add_str(rtext,"&amp;");
    } else {
      xmls_add_char(rtext,*s);
    }
    ++s;
  }
}

char *_htags[]={"img","meta","hr","br","link","input",NULL};

static int is_html_empty_tag(char *name)
{
  unsigned i;
  for(i=0;_htags[i];++i) {
    if(0==xml_strcasecmp(name,_htags[i]))
      return 1;
  }
  return 0;
}


void output_node_rec(XMLNODE *node, XMLSTRING rtext, TRANSFORM_CONTEXT *ctx)
{
  XMLNODE *attr;

  while(node) {
    switch(node->type) {
      case ELEMENT_NODE:
        xmls_add_char(rtext,'<');
        xmls_add_str(rtext, node->name);
        for(attr=node->attributes;attr;attr=attr->next) {
          xmls_add_char(rtext,' ');
          xmls_add_str(rtext, attr->name);
          xmls_add_str(rtext, "=\"");
          add_quoted_str(rtext, attr->content);
          xmls_add_char(rtext,'"');
        }
        if(ctx->outmode==MODE_HTML && is_html_empty_tag(node->name)) {
          xmls_add_str(rtext,">");
        } else {
          if(node->children) {
            if(ctx->rawout ||(ctx->outmode==MODE_HTML && 0==xml_strcasecmp(node->name,"script"))){
              ++ctx->rawout;
            }
            xmls_add_char(rtext,'>');
            output_node_rec(node->children, rtext, ctx);
            if(ctx->rawout){
              --ctx->rawout;
            }
            xmls_add_str(rtext, "</");
            xmls_add_str(rtext, node->name);
            xmls_add_char(rtext,'>');
          } else {
            if(ctx->outmode==MODE_XML) {
              xmls_add_str(rtext,"/>");
            } else {
              xmls_add_str(rtext, "></");
              xmls_add_str(rtext, node->name);
              xmls_add_char(rtext,'>');
            }
          }
        }
        break;
      case COMMENT_NODE:
        xmls_add_str(rtext,"<!--");
        if(node->content) {
          xmls_add_str(rtext, (char *)(node->content));
        }
        xmls_add_str(rtext,"-->");
        break;
      case PI_NODE:
        xmls_add_str(rtext,"<?");
        xmls_add_str(rtext, node->name);
        if(node->content) {
          xmls_add_char(rtext,' ');
          xmls_add_str(rtext, (char *)(node->content));
        }
        xmls_add_char(rtext,'>');
        break;
      case TEXT_NODE:
        if(node->flags & XML_FLAG_CDATA)
          xmls_add_str(rtext, "<![CDATA[");
        if((node->flags & XML_FLAG_NOESCAPE)||(ctx->rawout)) {
          xmls_add_str(rtext, (char *)(node->content));
        } else {
          add_quoted_str(rtext, (char *)(node->content));
        }
        if(node->flags & XML_FLAG_CDATA)
          xmls_add_str(rtext, "]]>");
        break;
      default:
        output_node_rec(node->children, rtext, ctx);
    }
    node = node->next;
  }
}

char  *XMLOutput(TRANSFORM_CONTEXT *ctx, XMLNODE *tree)
{
  XMLSTRING rtext = xmls_new(10000);
  XMLNODE *t;
  if(ctx->outmode==MODE_XML) {
    if(!(ctx->flags & XSL_FLAG_OMIT_DESC)) {
      xmls_add_str(rtext, "<?xml version=\"1.0\"");
      if(ctx->encoding)
        xmls_add_str(rtext, " encoding=\"UTF-8\"");
      if(ctx->flags&XSL_FLAG_STANDALONE)
        xmls_add_str(rtext, " standalone=\"yes\"");
      xmls_add_str(rtext, "?>\n");
    }
  }

  if(ctx->doctype_public && ctx->doctype_system) {
    t = find_first_node(tree);
    xmls_add_str(rtext, "<!DOCTYPE ");
    xmls_add_str(rtext, t->name);
    xmls_add_str(rtext, " PUBLIC \"");
    xmls_add_str(rtext, ctx->doctype_public?ctx->doctype_public:"-//W3C//DTD XHTML+RDFa 1.0//EN");
    xmls_add_str(rtext, "\" \"");
    xmls_add_str(rtext, ctx->doctype_system?ctx->doctype_system:"http://www.w3.org/MarkUp/DTD/xhtml-rdfa-1.dtd");
    xmls_add_str(rtext, "\">\n");
  }
  if(tree)
    output_node_rec(tree,rtext,ctx);
  return xmls_detach(rtext);
}

void XMLOutputFile(TRANSFORM_CONTEXT *ctx, XMLNODE *tree, char *filename)
{
  FILE *f = fopen(filename,"w");
  if(!f) {
    fprintf(stderr,"failed to create output file %s\n",filename);
    return;
  }
  char *result = XMLOutput(ctx, tree);
  if(result)
  {
    fputs(result, f);
    fputs("\n",f);
  }
  fclose(f);
  free(result);
}