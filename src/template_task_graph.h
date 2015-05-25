#ifndef TEMPLATE_TASK_GRAPH_H_
#define TEMPLATE_TASK_GRAPH_H_

#include "ltr_xsl.h"

typedef struct template_task_graph_ template_task_graph_t;

template_task_graph_t *template_task_graph_create(XMLSTRING filename);

void template_task_graph_release(template_task_graph_t *graph);

void template_task_graph_set_current(template_task_graph_t *graph, template_context *task);

void template_task_graph_add_parallel(template_task_graph_t *graph, XMLNODE *instruction, template_context *task);

void template_task_graph_add_serial(template_task_graph_t *graph, XMLNODE *instruction, template_context *task);

void template_task_graph_save(TRANSFORM_CONTEXT *context, template_task_graph_t *graph);

#endif
