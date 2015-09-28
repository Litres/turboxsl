/*
 *  TurboXSL XML+XSLT processing library
 *  XML nodeset to text convertor,
 *  file output.
 *
 *  (c) Egor Voznessenski, voznyak@mail.ru
 *
 *  $Id$
 *
**/

#include <stdio.h>

#include "ltr_xsl.h"
#include "xsl_elements.h"

void add_quoted_str(XMLSTRING rtext, char *s)
{
  while(*s) {
    if(*s=='<') {
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

static int is_html_empty_tag(XMLSTRING name)
{
  if(xmls_equals(name, xsl_s_img)) return 1;
  if(xmls_equals(name, xsl_s_meta)) return 1;
  if(xmls_equals(name, xsl_s_hr)) return 1;
  if(xmls_equals(name, xsl_s_br)) return 1;
  if(xmls_equals(name, xsl_s_link)) return 1;
  if(xmls_equals(name, xsl_s_input)) return 1;

  return 0;
}

void output_node_rec(XMLNODE *node, XMLSTRING rtext, TRANSFORM_CONTEXT *ctx)
{
  XMLNODE *attr;

  while(node) {
    switch(node->type) {
      case ELEMENT_NODE:
        xmls_add_char(rtext,'<');
        xmls_append(rtext, node->name);
        for(attr=node->attributes;attr;attr=attr->next) {
          xmls_add_char(rtext,' ');
          xmls_append(rtext, attr->name);
          xmls_add_str(rtext, "=\"");
          xmls_append(rtext, attr->content);
          xmls_add_char(rtext,'"');
        }
        if(ctx->outmode==MODE_HTML && is_html_empty_tag(node->name)) {
          xmls_add_str(rtext,">");
        } else {
          if(node->children) {
            if(ctx->rawout || (ctx->outmode==MODE_HTML && xmls_equals(node->name,xsl_s_script))) {
              ++ctx->rawout;
            }
            xmls_add_char(rtext,'>');
            output_node_rec(node->children, rtext, ctx);
            if(ctx->rawout){
              --ctx->rawout;
            }
            xmls_add_str(rtext, "</");
            xmls_append(rtext, node->name);
            xmls_add_char(rtext,'>');
          } else {
            if(ctx->outmode==MODE_XML) {
              xmls_add_str(rtext,"/>");
            } else {
              xmls_add_str(rtext, "></");
              xmls_append(rtext, node->name);
              xmls_add_char(rtext,'>');
            }
          }
        }
        break;
      case COMMENT_NODE:
        xmls_add_str(rtext,"<!--");
        if(node->content) {
          xmls_append(rtext, node->content);
        }
        xmls_add_str(rtext,"-->");
        break;
      case PI_NODE:
        xmls_add_str(rtext,"<?");
        xmls_append(rtext, node->name);
        if(node->content) {
          xmls_add_char(rtext,' ');
          xmls_append(rtext, node->content);
        }
        xmls_add_char(rtext,'>');
        break;
      case TEXT_NODE:
        if(node->flags & XML_FLAG_CDATA)
          xmls_add_str(rtext, "<![CDATA[");
        if((node->flags & XML_FLAG_NOESCAPE)||(ctx->rawout)) {
          xmls_append(rtext, node->content);
        } else {
          if(node->content != NULL) add_quoted_str(rtext, node->content->s);
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

char *XMLOutput(TRANSFORM_CONTEXT *ctx, XMLNODE *tree)
{
  XMLSTRING rtext = xmls_new(10000);
  XMLNODE *t;
  if(ctx->outmode==MODE_XML) {
    if(!(ctx->flags & XSL_FLAG_OMIT_DESC)) {
      xmls_add_str(rtext, "<?xml version=\"1.0\"");
      if(ctx->encoding)
        xmls_add_str(rtext, " encoding=\"UTF-8\"");
      if(ctx->flags & XSL_FLAG_STANDALONE)
        xmls_add_str(rtext, " standalone=\"yes\"");
      xmls_add_str(rtext, "?>\n");
    }
  }

  if(ctx->doctype_public && ctx->doctype_system) {
    t = find_first_node(tree);
    if (t != NULL)
    {
      xmls_add_str(rtext, "<!DOCTYPE ");
      xmls_append(rtext, t->name);
      xmls_add_str(rtext, " PUBLIC \"");
      xmls_add_str(rtext, ctx->doctype_public ? ctx->doctype_public->s : "-//W3C//DTD XHTML+RDFa 1.0//EN");
      xmls_add_str(rtext, "\" \"");
      xmls_add_str(rtext, ctx->doctype_system ? ctx->doctype_system->s : "http://www.w3.org/MarkUp/DTD/xhtml-rdfa-1.dtd");
      xmls_add_str(rtext, "\">\n");
    }
    else
    {
      error("XMLOutput:: first node not found");
    }
  }
  if(tree)
    output_node_rec(tree,rtext,ctx);
  return xmls_detach(rtext);
}

void XMLOutputFile(TRANSFORM_CONTEXT *ctx, XMLNODE *tree, char *filename)
{
  FILE *f = fopen(filename,"w");
  if(!f) {
    error("XMLOutputFile:: failed to create output file %s",filename);
    return;
  }
  char *result = XMLOutput(ctx, tree);
  if(result)
  {
    fputs(result, f);
    fputs("\n",f);
  }
  fclose(f);
}
