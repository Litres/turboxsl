/*
 *  TurboXSL XML+XSLT processing library
 *  Main transformation loop + interface functions
 *
 *
 *  (c) Egor Voznessenski, voznyak@mail.ru
 *
 *  $Id$
 *
**/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ltr_xsl.h"
#include "thread_lock.h"

#include "xsl_elements.h"
#include "template_task.h"
#include "template_task_graph.h"
#include "instructions.h"

XMLNODE *copy_node_to_result(TRANSFORM_CONTEXT *pctx, XMLNODE *locals, XMLNODE *context, XMLNODE *parent, XMLNODE *src)
{
  XMLNODE *newnode = xml_append_child(pctx,parent,src->type);
  newnode->name  = src->name;
  newnode->flags = src->flags;

  if(src->type == ELEMENT_NODE) {
    for(XMLNODE *a = src->attributes; a; a = a->next) {
      XMLNODE *t = xml_new_node(pctx,NULL,ATTRIBUTE_NODE);
      t->name = a->name;
      t->content = xml_process_string(pctx, locals, context, a->content);
      t->next = newnode->attributes;
      newnode->attributes = t;
    }
  }

  if(src->type == TEXT_NODE) {
    newnode->content = xmls_new_string_literal(src->content->s);
  }

  return newnode;
}

void copy_node_to_result_rec(TRANSFORM_CONTEXT *pctx, XMLNODE *locals, XMLNODE *context, XMLNODE *parent, XMLNODE *src)
{
  XMLNODE *r, *c;
  if(src->type==ATTRIBUTE_NODE) {
    r = xml_new_node(pctx,NULL,ATTRIBUTE_NODE);
    r->name = src->name;
    r->next = parent->attributes;
    parent->attributes = r;
    r->content = src->content;
    src->content = NULL; // to prevent double free
  } else {
    r = copy_node_to_result(pctx,locals,context,parent,src);
    for(c=src->children;c;c=c->next) {
      copy_node_to_result_rec(pctx,locals,context,r,c);
    }
  }
}

XMLSTRING xml_eval_string(TRANSFORM_CONTEXT *pctx, XMLNODE *locals, XMLNODE *source, XMLNODE *foreval)
{
  XMLNODE *tmp = xml_new_node(pctx, NULL, EMPTY_NODE);

  template_context *new_context = memory_allocator_new(sizeof(template_context));
  new_context->context = pctx;
  new_context->instruction = foreval;
  new_context->result = tmp;
  new_context->document_node = source;
  new_context->local_variables = locals;

  apply_xslt_template(new_context);

  XMLSTRING res = xmls_new(200);
  output_node_rec(tmp, res, pctx);

  return res;
}

void xml_add_attribute(TRANSFORM_CONTEXT *pctx, XMLNODE *parent, XMLSTRING name, char *value)
{
  while(parent && parent->type == EMPTY_NODE) {
    parent = parent->parent;
  }
  if(!parent) return;

  XMLNODE *tmp;
  for(tmp = parent->attributes; tmp; tmp = tmp->next) {
    if(xmls_equals(tmp->name, name)) break;
  }

  if(!tmp) {
    tmp = xml_new_node(pctx, NULL, ATTRIBUTE_NODE);
    tmp->name = name;
    tmp->next = parent->attributes;
    parent->attributes = tmp;
  }
  tmp->content = xmls_new_string_literal(value);
}  

void apply_xslt_template(template_context *context)
{
  for(XMLNODE *instr = context->instruction; instr; instr = instr->next) {
    if(instr->type == EMPTY_NODE) {
      if(instr->children) {
        context->instruction = instr->children;
        apply_xslt_template(context);
      }
      continue;
    }

    if(instructions_is_xsl(instr)) {
      instructions_process(context, instr);
    } else {
      XMLNODE *result = copy_node_to_result(context->context, context->local_variables, context->document_node, context->result, instr);
      if(instr->children) {
        template_context *new_context = memory_allocator_new(sizeof(template_context));
        new_context->context = context->context;
        new_context->instruction = instr->children;
        new_context->result = result;
        new_context->document_node = context->document_node;
        new_context->parameters = context->parameters;
        new_context->local_variables = copy_variables(context->context, context->local_variables);
        new_context->workers = context->workers;
        new_context->task_mode = context->task_mode;

        apply_xslt_template(new_context);
      }
    }
  }
}

void apply_default_process(template_context *context)
{
  if(context->document_node->type == TEXT_NODE) {
    XMLNODE *tmp = xml_append_child(context->context, context->result, TEXT_NODE);
    tmp->content = xmls_new_string_literal(context->document_node->content->s);
    return;
  }

  for(XMLNODE *child = context->document_node->children; child; child = child->next) {
    if(child->type == TEXT_NODE) {
      XMLNODE *tmp = xml_append_child(context->context, context->result, TEXT_NODE);
      tmp->content = xmls_new_string_literal(child->content->s);
    } else {
      XMLNODE *result = xml_append_child(context->context, context->result, EMPTY_NODE);

      template_context *new_context = memory_allocator_new(sizeof(template_context));
      new_context->context = context->context;
      new_context->result = result;
      new_context->document_node = child;
      new_context->parameters = context->parameters;
      new_context->local_variables = copy_variables(context->context, context->local_variables);
      new_context->mode = context->mode;
      new_context->workers = context->workers;
      new_context->task_mode = context->task_mode;

      process_one_node(new_context);
    }
  }
}

void process_one_node(template_context *context)
{
  if(!context->document_node) return;

  XMLNODE *template = find_template(context->context, context->document_node, context->mode);
  if(template) {
    // user scope locals for each template
    XMLNODE *scope = xml_new_node(context->context, NULL, EMPTY_NODE);

    template_context *new_context = memory_allocator_new(sizeof(template_context));
    new_context->context = context->context;
    new_context->instruction = template;
    new_context->result = context->result;
    new_context->document_node = context->document_node;
    new_context->parameters = context->parameters;
    new_context->local_variables = scope;
    new_context->workers = context->workers;
    new_context->task_mode = context->task_mode;

    apply_xslt_template(new_context);
  } else {
    apply_default_process(context);
  }
}

char *get_absolute_path(XMLNODE *node, char *name)
{
  char *file = node->file;
  char *p = strrchr(file, '/');
  if(p != NULL) {
    size_t path_length = p - file + 1;
    char *result = memory_allocator_new(path_length + strlen(name) + 1);
    memcpy(result, file, path_length);
    memcpy(result + path_length, name, strlen(name));
    return result;
  }

  return name;
}

char *get_reference_path(XMLNODE *node)
{
  XMLSTRING reference = xml_get_attr(node, xsl_a_href);
  if(reference == NULL) return NULL;

  return get_absolute_path(node, reference->s);
}

XMLNODE *xsl_preprocess(TRANSFORM_CONTEXT *pctx, XMLNODE *node)
{
  XMLNODE *ret = node;
  XMLNODE *node_next;
  XMLSTRING pp;

  while(node) {
    node_next = node->next;

    if(xmls_equals(node->name, xsl_output) && !(pctx->flags & XSL_FLAG_OUTPUT)) {
      pctx->flags |= XSL_FLAG_OUTPUT;
      pctx->doctype_public = xml_get_attr(node,xsl_a_dtpublic);
      pctx->doctype_system = xml_get_attr(node,xsl_a_dtsystem);
      pctx->encoding = xml_get_attr(node,xsl_a_encoding);
      pctx->media_type = xml_get_attr(node,xsl_a_media);
      pp = xml_get_attr(node,xsl_a_method);
      if(pp) {
        if(xmls_equals(pp,xsl_s_xml))
          pctx->outmode = MODE_XML;
        else if(xmls_equals(pp,xsl_s_html))
          pctx->outmode = MODE_HTML;
        else if(xmls_equals(pp,xsl_s_text))
          pctx->outmode = MODE_TEXT;
        pctx->flags|=XSL_FLAG_MODE_SET;
      }

      pp = xml_get_attr(node,xsl_a_omitxml);
      if(xmls_equals(pp,xsl_s_yes))
        pctx->flags |= XSL_FLAG_OMIT_DESC;

      pp = xml_get_attr(node,xsl_a_standalone);
      if(xmls_equals(pp,xsl_s_yes))
        pctx->flags |= XSL_FLAG_STANDALONE;

    } else if(xmls_equals(node->name, xsl_include)) {
      char *name = get_reference_path(node);
      if(name) {
        XMLNODE *included = xml_parse_file(pctx->gctx, name, 1);
        if(included) {
          included->parent = node;
          node->type = EMPTY_NODE;
          node->children = included;
        } else {
          error("xsl_preprocess:: failed to include %s", name);
        }
      }
    }

    if(node->type == TEXT_NODE && !xmls_equals(node->parent->name, xsl_text)) {
      char *p = node->content->s;
      for(;*p;++p) {
        if(!x_is_ws(*p))
          break;
      }
      if(*p==0) {
        node->content = NULL;
        xml_unlink_node(node);
        if(node==ret)
          ret = node_next;
        node = node_next;
        continue;
      }
    }

    if(node->children) {
      node->children = xsl_preprocess(pctx, node->children);
    }
    node = node_next;
  }
  return ret;
}


void process_imports(TRANSFORM_CONTEXT *pctx, XMLNODE *node)
{
  for(;node;node=node->next) 
  {
    if(xmls_equals(node->name, xsl_import))
    {
      char *name = get_reference_path(node);
      if(name) 
      {
       	debug("process_imports:: import %s", name);
        XMLNODE *included = xml_parse_file(pctx->gctx, name, 1);
        if(included != NULL) 
        {
          xsl_preprocess(pctx, included);  //process includes and clean-up
          process_imports(pctx, included); //process imports if any
          node->type = EMPTY_NODE;
          node->children = included;
        }
        else {
          error("process_imports:: failed to import %s", name);
        }
      }
    }
    else if(node->children) {
      process_imports(pctx,node->children); //process imports if any
    }
  }
}

void preformat_document(TRANSFORM_CONTEXT *pctx, XMLNODE *doc)
{
    // TODO apply strip-space
}

void process_global_flags(TRANSFORM_CONTEXT *pctx, XMLNODE *node)
{
  XMLSTRING name;
  XMLNODE *tmp;

  while(node) {
    // disable escaping for text and value-of
    if(xmls_equals(node->name, xsl_text) || xmls_equals(node->name, xsl_value_of)) {
      XMLSTRING dis = xml_get_attr(node,xsl_a_escaping);
      if(xmls_equals(dis,xsl_s_yes)) {
        node->flags |= XML_FLAG_NOESCAPE;
      }
    }
/*********** process xsl:key instructions ************/
    if(xmls_equals(node->name, xsl_key)) {
      name = xml_get_attr(node, xsl_a_name);
      tmp = xml_new_node(pctx, name, KEY_NODE);

      XMLSTRING match = xml_get_attr(node, xsl_a_match);
      XMLSTRING use = xml_get_attr(node, xsl_a_use);
      char *format = "%s[%s = '%%s']";
      int size = snprintf(NULL, 0, format, match->s, use->s);
      if (size > 0) {
        int buffer_size = size + 1;
        char *buffer = memory_allocator_new(buffer_size);
        if (snprintf(buffer, buffer_size, format, match->s, use->s) == size) {
          debug("process_global_flags:: key predicate: %s", buffer);
          tmp->content = xmls_new_string_literal(buffer);
        }
      }

      tmp->next = pctx->keys;
      pctx->keys = tmp;
    }
/*********** process xsl:sort instructions ************/
    else if(xmls_equals(node->name, xsl_sort)) {
      node->compiled = xpath_find_expr(pctx,xml_get_attr(node, xsl_a_select));
      name = xml_get_attr(node, xsl_a_datatype);
      if(xmls_equals(name, xsl_s_number))
        node->flags |= XML_FLAG_SORTNUMBER;
      name = xml_get_attr(node, xsl_a_order);
      if(xmls_equals(name, xsl_s_descending))
        node->flags |= XML_FLAG_DESCENDING;
      name = xml_get_attr(node, xsl_a_caseorder);
      if(xmls_equals(name, xsl_s_lower_first))
        node->flags |= XML_FLAG_LOWER;
    }
/*********** process xsl:decimal-format instructions ************/
    else if(xmls_equals(node->name, xsl_decimal)) {
      name = xml_get_attr(node, xsl_a_name);
      tmp = xml_new_node(pctx, name, ATTRIBUTE_NODE);
      tmp->children = node;
      tmp->next = pctx->formats;
      pctx->formats = tmp;
    }
    if(node->children)
      process_global_flags(pctx, node->children);
    node = node->next;
  }
}

/************************** Interface functions ******************************/

void XSLTEnd(XSLTGLOBALDATA *data)
{
  info("XSLTEnd");

  memory_allocator_release(data->allocator);
  dict_free(data->revisions);
  dict_free(data->group_rights);
  concurrent_dictionary_release(data->urldict);
  if (data->cache != NULL) external_cache_release(data->cache);

  free(data->perl_functions);
  free(data);

  instructions_release();

  logger_release();
}

XSLTGLOBALDATA *XSLTInit(void *interpreter)
{
  logger_create();

  info("XSLTInit");

  XSLTGLOBALDATA *ret = malloc(sizeof(XSLTGLOBALDATA));
  memset(ret,0,sizeof(XSLTGLOBALDATA));

  ret->allocator = memory_allocator_create(NULL);
  memory_allocator_add_entry(ret->allocator, pthread_self(), 1000000);
  memory_allocator_set_current(ret->allocator);

  xsl_elements_setup();
  instructions_create();

  ret->urldict = concurrent_dictionary_create();
  ret->revisions = dict_new(300);
  ret->group_rights = dict_new(300);
  ret->interpreter = interpreter;

  return ret;
}

void XSLTAddURLRevision(XSLTGLOBALDATA *data, char *url, char *revision)
{
  memory_allocator_set_current(data->allocator);
  dict_add(data->revisions, xmls_new_string_literal(url), xml_strdup(revision));
}

void XSLTEnableExternalCache(XSLTGLOBALDATA *data, char *server_list)
{
  memory_allocator_set_current(data->allocator);
  data->cache = external_cache_create(xml_strdup(server_list));
}

void xml_free_document(XMLNODE *doc)
{
  debug("xml_free_document:: file %s", doc->file);

  XMLNODE *children = doc->children;
  if (children != NULL) xml_free_document(children);

  if (doc->allocator != NULL) memory_allocator_release(doc->allocator);
}

void XMLFreeDocument(XMLNODE *doc)
{
  info("XMLFreeDocument:: file: %s", doc->file);
  xml_free_document(doc);
}

void XSLTFreeProcessor(TRANSFORM_CONTEXT *pctx)
{
  info("XSLTFreeProcessor");

  memory_allocator_set_current(pctx->allocator);

  concurrent_dictionary_release(pctx->expressions);
  template_map_release(pctx->templates);
  dict_free(pctx->named_templ);
  dict_free(pctx->parallel_instructions);
  template_task_graph_release(pctx->task_graph);
  threadpool_free(pctx->pool);

  xml_free_document(pctx->stylesheet);

  memory_allocator_release(pctx->allocator);

  free(pctx->sort_keys);
  free(pctx->sort_nodes);
  free(pctx->functions);
  free(pctx);
}

TRANSFORM_CONTEXT *XSLTNewProcessor(XSLTGLOBALDATA *gctx, char *stylesheet)
{
  info("XSLTNewProcessor:: stylesheet %s", stylesheet);

  TRANSFORM_CONTEXT *ret = malloc(sizeof(TRANSFORM_CONTEXT));
  if(!ret) {
    error("XSLTNewProcessor:: create context");
    return NULL;
  }
  memset(ret,0,sizeof(TRANSFORM_CONTEXT));

  if (!thread_lock_create_recursive(&(ret->lock)))
  {
    return NULL;
  }

  ret->allocator = memory_allocator_create(NULL);
  if (ret->allocator == NULL)
  {
      return NULL;
  }
  memory_allocator_add_entry(ret->allocator, pthread_self(), 1000000);
  memory_allocator_set_current(ret->allocator);

  ret->gctx = gctx;
  ret->stylesheet = xml_parse_file(gctx, xml_strdup(stylesheet), 1);
  if(!ret->stylesheet) {
    free(ret);
    return NULL;
  }

  ret->outmode = MODE_XML;
  ret->sort_size = 100;
  ret->sort_keys = (char **)malloc(ret->sort_size*sizeof(char *));
  ret->sort_nodes = (XMLNODE **)malloc(ret->sort_size*sizeof(XMLNODE *));

  ret->templates = template_map_create();
  ret->named_templ = dict_new(300);
  ret->expressions = concurrent_dictionary_create();

  xpath_setup_functions(ret);
  instructions_set_parallel(ret);

  ret->stylesheet = xsl_preprocess(ret, ret->stylesheet);  //root node is always empty
  process_imports(ret, ret->stylesheet->children);
  precompile_templates(ret, ret->stylesheet);
  process_global_flags(ret, ret->stylesheet);

  return ret;
}

XMLNODE *find_first_node(XMLNODE *n)
{
  for(;n;n=n->next) {
    if(n->type != EMPTY_NODE)
      return n;
    XMLNODE *t=find_first_node(n->children);
    if(t != NULL) return t;
  }
  return NULL;
}

XMLNODE *find_node_by_name(XMLNODE *root, XMLSTRING name)
{
  for(XMLNODE *n = root; n; n = n->next) {
    if(xmls_equals(n->name, name)) return n;
    XMLNODE *t = find_node_by_name(n->children, name);
    if(t != NULL) return t;
  }
  return NULL;
}

void XSLTCreateThreadPool(TRANSFORM_CONTEXT *pctx, unsigned int size)
{
  if (pctx->pool != NULL)
  {
    error("XSLTCreateThreadPool:: thread pool exists");
    return;
  }

  memory_allocator_set_current(pctx->allocator);

  pctx->pool = threadpool_init(size);
  threadpool_set_allocator(pctx->allocator, pctx->pool);

  if (pctx->gctx->cache != NULL) threadpool_set_external_cache(pctx->gctx->cache, pctx->pool);
}

void XSLTSetParallelInstructions(TRANSFORM_CONTEXT *ctx, char **tags, int tag_count)
{
  memory_allocator_set_current(ctx->allocator);

  dict_free(ctx->parallel_instructions);
  ctx->parallel_instructions = dict_new(32);

  for (int i = 0; i < tag_count; i++)
  {
    XMLSTRING tag_string = xmls_new_string_literal(tags[i]);
    dict_add(ctx->parallel_instructions, tag_string, tag_string);
  }
}

void XSLTSetCacheKeyPrefix(TRANSFORM_CONTEXT *ctx, char *prefix)
{
  memory_allocator_set_current(ctx->allocator);
  ctx->cache_key_prefix = xml_strdup(prefix);
}

void XSLTSetURLLocalPrefix(TRANSFORM_CONTEXT *ctx, char *prefix)
{
  memory_allocator_set_current(ctx->allocator);
  ctx->url_local_prefix = xml_strdup(prefix);
}

void XSLTEnableTaskGraph(TRANSFORM_CONTEXT *ctx, char *filename)
{
  memory_allocator_set_current(ctx->allocator);
  ctx->task_graph = template_task_graph_create(xmls_new_string_literal(filename));
}

XMLNODE *XSLTProcess(TRANSFORM_CONTEXT *pctx, XMLNODE *document)
{
  if(!pctx) {
    error("XSLTProcess:: pctx is NULL");
    return NULL;
  }

  if(!document) {
    error("XSLTProcess:: document is NULL");
    return NULL;
  }

  // memory allocator for output document
  memory_allocator *allocator = memory_allocator_create(pctx->allocator);
  memory_allocator_add_entry(allocator, pthread_self(), 1000000);
  memory_allocator_set_current(allocator);

  XMLNODE *ret = xml_new_node(pctx, xmls_new_string_literal("out"), EMPTY_NODE);
  ret->allocator = allocator;

  pctx->root_node = document;
  // temporary turn off pool
  threadpool *pool = pctx->pool;
  pctx->pool = NULL;

  precompile_variables(pctx, pctx->stylesheet->children, document);
  preformat_document(pctx,document);

  // turn on pool
  pctx->pool = pool;
  threadpool_set_allocator(allocator, pctx->pool);

  template_context *new_context = memory_allocator_new(sizeof(template_context));
  new_context->context = pctx;
  new_context->result = ret;
  new_context->document_node = document;
  new_context->local_variables = xml_new_node(pctx, NULL, EMPTY_NODE);

  info("XSLTProcess:: start process");
  template_task_run_and_wait(new_context, process_one_node);
  info("XSLTProcess:: end process");

  template_task_graph_save(pctx, pctx->task_graph);

/****************** add dtd et al if required *******************/
  XMLNODE *t = find_first_node(ret);
  if(pctx->outmode == MODE_XML && !(pctx->flags & XSL_FLAG_MODE_SET)) {
    pctx->outmode = MODE_XML;
    if(t != NULL && t->type == ELEMENT_NODE) {
      if(xmls_equals(t->name, xsl_s_html)) pctx->outmode = MODE_HTML;
    }
  }

  if(pctx->outmode == MODE_HTML) {
    XMLNODE *head = find_node_by_name(ret, xsl_s_head);
    if(head) {
      XMLNODE *meta = xml_new_node(pctx, NULL,TEXT_NODE);
      meta->content = xmls_new_string_literal("<meta http-equiv=\"Content-Type\" content=\"text/html; charset=UTF-8\">");
      meta->flags = XML_FLAG_NOESCAPE;
      meta->next = head->children;
      if(head->children) {
        head->children->prev = meta;
        meta->parent = head->children->parent;
        if(head->children->parent)
          head->children->parent->children = meta;
      }
    }
  }

  free_variables(pctx);

  return ret;
}
