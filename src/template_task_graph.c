#include "template_task_graph.h"

#include "thread_lock.h"
#include "xsl_elements.h"

typedef struct template_task_graph_vertex_ {
    int id;
    XMLSTRING key;
    XMLSTRING color;
    struct template_task_graph_vertex_ *next;
} template_task_graph_vertex_t;

typedef struct template_task_graph_edge_ {
    XMLSTRING label;
    template_task_graph_vertex_t *source;
    template_task_graph_vertex_t *target;
    struct template_task_graph_edge_ *next;
} template_task_graph_edge_t;

struct template_task_graph_ {
    XMLSTRING filename;
    pthread_key_t vertex_key;
    pthread_mutex_t lock;
    template_task_graph_edge_t *head;
    template_task_graph_edge_t *tail;
};

XMLSTRING template_task_graph_vertex_key(template_context *task)
{
    char buffer[64];
    sprintf(buffer, "%p", task);
    return xmls_new_string_literal(buffer);
}

XMLSTRING template_task_graph_edge_name(XMLNODE *instruction)
{
    XMLSTRING result = xmls_new(128);
    xmls_append(result, instruction->name);

    XMLSTRING fork = xml_get_attr(instruction, xsl_a_fork);
    if (fork != NULL)
    {
        xmls_add_str(result, " [");
        xmls_append(result, fork);
        xmls_add_str(result, "]");
    }

    return result;
}

template_task_graph_vertex_t *template_task_graph_vertexes(template_task_graph_t *graph)
{
    int id = 1;
    template_task_graph_vertex_t *head = NULL;
    template_task_graph_vertex_t *tail = NULL;

    XMLDICT *vertexes = dict_new(128);
    template_task_graph_edge_t *edge = graph->head;
    while (edge != NULL)
    {
        template_task_graph_vertex_t *source = (template_task_graph_vertex_t *) dict_find(vertexes, edge->source->key);
        if (source == NULL)
        {
            dict_add(vertexes, edge->source->key, edge->source);
            edge->source->id = id++;
            if (head == NULL)
            {
                head = edge->source;
                tail = head;
            }
            else
            {
                tail->next = edge->source;
                tail = edge->source;
            }
        }
        else
        {
            edge->source = source;
        }

        template_task_graph_vertex_t *target = (template_task_graph_vertex_t *) dict_find(vertexes, edge->target->key);
        if (target == NULL)
        {
            dict_add(vertexes, edge->target->key, edge->target);
            edge->target->id = id++;
            if (head == NULL)
            {
                head = edge->target;
                tail = head;
            }
            else
            {
                tail->next = edge->target;
                tail = edge->target;
            }
        }
        else
        {
            edge->target = target;
        }

        edge = edge->next;
    }

    dict_free(vertexes);

    return head;
}

template_task_graph_t *template_task_graph_create(XMLSTRING filename)
{
    template_task_graph_t *result = memory_allocator_new(sizeof(template_task_graph_t));
    result->filename = filename;

    thread_lock_create_recursive(&(result->lock));

    if (pthread_key_create(&(result->vertex_key), NULL))
    {
        error("template_task_graph_create:: key");
        return NULL;
    }

    return result;
}

void template_task_graph_release(template_task_graph_t *graph)
{
    if (graph == NULL) return;

    pthread_mutex_destroy(&(graph->lock));
    pthread_key_delete(graph->vertex_key);
}

void template_task_graph_set_current(template_task_graph_t *graph, template_context *task)
{
    if (graph == NULL) return;

    template_task_graph_vertex_t *v = memory_allocator_new(sizeof(template_task_graph_vertex_t));
    v->key = template_task_graph_vertex_key(task);
    pthread_setspecific(graph->vertex_key, v);
}

void template_task_graph_add(template_task_graph_t *graph, XMLNODE *instruction, template_context *task, int mode)
{
    if (graph == NULL) return;

    template_task_graph_vertex_t *source = (template_task_graph_vertex_t *) pthread_getspecific(graph->vertex_key);
    if (source == NULL)
    {
        error("template_task_graph_add:: task not found");
        return;
    }

    template_task_graph_vertex_t *target = memory_allocator_new(sizeof(template_task_graph_vertex_t));
    target->key = template_task_graph_vertex_key(task);
    if (mode == 0) target->color = xsl_s_red;
    if (mode == 1) target->color = xsl_s_green;

    template_task_graph_edge_t *edge = memory_allocator_new(sizeof(template_task_graph_edge_t));
    edge->label = template_task_graph_edge_name(instruction);
    edge->source = source;
    edge->target = target;

    if (pthread_mutex_lock(&(graph->lock)))
    {
        error("template_task_graph_add:: lock");
        return;
    }

    if (graph->head == NULL)
    {
        graph->head = edge;
        graph->tail = graph->head;
    }
    else
    {
        graph->tail->next = edge;
        graph->tail = edge;
    }

    pthread_mutex_unlock(&(graph->lock));
}

void template_task_graph_add_parallel(template_task_graph_t *graph, XMLNODE *instruction, template_context *task)
{
    template_task_graph_add(graph, instruction, task, 1);
}

void template_task_graph_add_serial(template_task_graph_t *graph, XMLNODE *instruction, template_context *task)
{
    template_task_graph_add(graph, instruction, task, 0);
}

XMLNODE *template_task_graph_color_key(TRANSFORM_CONTEXT *context)
{
    XMLNODE *result = xml_new_node(context, xmls_new_string_literal("key"), ELEMENT_NODE);

    XMLNODE *id_attribute = xml_new_node(context, xmls_new_string_literal("id"), ATTRIBUTE_NODE);
    id_attribute->content = xmls_new_string_literal("d0");
    id_attribute->next = result->attributes;
    result->attributes = id_attribute;

    XMLNODE *for_attribute = xml_new_node(context, xmls_new_string_literal("for"), ATTRIBUTE_NODE);
    for_attribute->content = xmls_new_string_literal("node");
    for_attribute->next = result->attributes;
    result->attributes = for_attribute;

    XMLNODE *name_attribute = xml_new_node(context, xmls_new_string_literal("attr.name"), ATTRIBUTE_NODE);
    name_attribute->content = xmls_new_string_literal("color");
    name_attribute->next = result->attributes;
    result->attributes = name_attribute;

    XMLNODE *type_attribute = xml_new_node(context, xmls_new_string_literal("attr.type"), ATTRIBUTE_NODE);
    type_attribute->content = xmls_new_string_literal("string");
    type_attribute->next = result->attributes;
    result->attributes = type_attribute;

    return result;
}

XMLNODE *template_task_graph_name_key(TRANSFORM_CONTEXT *context)
{
    XMLNODE *result = xml_new_node(context, xmls_new_string_literal("key"), ELEMENT_NODE);

    XMLNODE *id_attribute = xml_new_node(context, xmls_new_string_literal("id"), ATTRIBUTE_NODE);
    id_attribute->content = xmls_new_string_literal("d1");
    id_attribute->next = result->attributes;
    result->attributes = id_attribute;

    XMLNODE *for_attribute = xml_new_node(context, xmls_new_string_literal("for"), ATTRIBUTE_NODE);
    for_attribute->content = xmls_new_string_literal("edge");
    for_attribute->next = result->attributes;
    result->attributes = for_attribute;

    XMLNODE *name_attribute = xml_new_node(context, xmls_new_string_literal("attr.name"), ATTRIBUTE_NODE);
    name_attribute->content = xmls_new_string_literal("name");
    name_attribute->next = result->attributes;
    result->attributes = name_attribute;

    XMLNODE *type_attribute = xml_new_node(context, xmls_new_string_literal("attr.type"), ATTRIBUTE_NODE);
    type_attribute->content = xmls_new_string_literal("string");
    type_attribute->next = result->attributes;
    result->attributes = type_attribute;

    return result;
}

XMLNODE *template_task_graph_color_data(TRANSFORM_CONTEXT *context, template_task_graph_vertex_t *vertex)
{
    XMLNODE *result = xml_new_node(context, xmls_new_string_literal("data"), ELEMENT_NODE);

    XMLNODE *key_attribute = xml_new_node(context, xmls_new_string_literal("key"), ATTRIBUTE_NODE);
    key_attribute->content = xmls_new_string_literal("d0");
    key_attribute->next = result->attributes;
    result->attributes = key_attribute;

    XMLNODE *text = xml_new_node(context, NULL, TEXT_NODE);
    text->content = vertex->color == NULL ? xsl_s_yellow : vertex->color;

    result->children = text;

    return result;
}

XMLNODE *template_task_graph_name_data(TRANSFORM_CONTEXT *context, template_task_graph_edge_t *edge)
{
    XMLNODE *result = xml_new_node(context, xmls_new_string_literal("data"), ELEMENT_NODE);

    XMLNODE *key_attribute = xml_new_node(context, xmls_new_string_literal("key"), ATTRIBUTE_NODE);
    key_attribute->content = xmls_new_string_literal("d1");
    key_attribute->next = result->attributes;
    result->attributes = key_attribute;

    XMLNODE *text = xml_new_node(context, NULL, TEXT_NODE);
    text->content = edge->label;

    result->children = text;

    return result;
}

void template_task_graph_save(TRANSFORM_CONTEXT *context, template_task_graph_t *graph)
{
    if (graph == NULL) return;

    XMLNODE *root = xml_new_node(context, xmls_new_string_literal("graphml"), ELEMENT_NODE);

    XMLNODE *edge_attribute = xml_new_node(context, xmls_new_string_literal("edgedefault"), ATTRIBUTE_NODE);
    edge_attribute->content = xmls_new_string_literal("directed");
    edge_attribute->next = root->attributes;
    root->attributes = edge_attribute;

    XMLNODE *last_child = NULL;

    root->children = template_task_graph_color_key(context);
    last_child = root->children;

    XMLNODE *name_key = template_task_graph_name_key(context);
    last_child->next = name_key;
    last_child = name_key;

    template_task_graph_vertex_t *vertex = template_task_graph_vertexes(graph);
    char buffer[128];
    while (vertex != NULL)
    {
        XMLNODE *node_element = xml_new_node(context, xmls_new_string_literal("node"), ELEMENT_NODE);

        XMLNODE *id_attribute = xml_new_node(context, xmls_new_string_literal("id"), ATTRIBUTE_NODE);
        memset(buffer, 0, sizeof(buffer));
        sprintf(buffer, "n%d", vertex->id);
        id_attribute->content = xmls_new_string_literal(buffer);
        id_attribute->next = node_element->attributes;
        node_element->attributes = id_attribute;

        node_element->children = template_task_graph_color_data(context, vertex);

        last_child->next = node_element;
        last_child = node_element;

        vertex = vertex->next;
    }

    int id = 1;
    template_task_graph_edge_t *edge = graph->head;
    while (edge != NULL)
    {
        XMLNODE *edge_element = xml_new_node(context, xmls_new_string_literal("edge"), ELEMENT_NODE);

        XMLNODE *id_attribute = xml_new_node(context, xmls_new_string_literal("id"), ATTRIBUTE_NODE);
        memset(buffer, 0, sizeof(buffer));
        sprintf(buffer, "e%d", id++);
        id_attribute->content = xmls_new_string_literal(buffer);
        id_attribute->next = edge_element->attributes;
        edge_element->attributes = id_attribute;

        XMLNODE *source_attribute = xml_new_node(context, xmls_new_string_literal("source"), ATTRIBUTE_NODE);
        memset(buffer, 0, sizeof(buffer));
        sprintf(buffer, "n%d", edge->source->id);
        source_attribute->content = xmls_new_string_literal(buffer);
        source_attribute->next = edge_element->attributes;
        edge_element->attributes = source_attribute;

        XMLNODE *target_attribute = xml_new_node(context, xmls_new_string_literal("target"), ATTRIBUTE_NODE);
        memset(buffer, 0, sizeof(buffer));
        sprintf(buffer, "n%d", edge->target->id);
        target_attribute->content = xmls_new_string_literal(buffer);
        target_attribute->next = edge_element->attributes;
        edge_element->attributes = target_attribute;

        edge_element->children = template_task_graph_name_data(context, edge);

        last_child->next = edge_element;
        last_child = edge_element;

        edge = edge->next;
    }

    // TODO another way?
    OUTPUT_MODE mode = context->outmode;
    XSL_FLAG flags = context->flags;
    XMLSTRING document_type = context->doctype_public;

    context->outmode = MODE_XML;
    context->flags = XSL_FLAG_OUTPUT;
    context->doctype_public = NULL;
    XMLOutputFile(context, root, graph->filename->s);

    context->outmode = mode;
    context->flags = flags;
    context->doctype_public = document_type;
}
