#include "template_task_graph.h"

#include "thread_lock.h"
#include "xsl_elements.h"

typedef struct template_task_graph_vertex_ {
    XMLSTRING key;
    int id;
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

XMLSTRING template_task_graph_key(template_context *task)
{
    char buffer[64];
    sprintf(buffer, "%p", task);
    return xmls_new_string_literal(buffer);
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
        if (dict_find(vertexes, edge->source->key) == NULL)
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

        if (dict_find(vertexes, edge->target->key) == NULL)
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

        edge = edge->next;
    }

    dict_free(vertexes);

    return head;
}

void template_task_graph_save(template_task_graph_t *graph)
{
    XMLSTRING result = xmls_new(1024);
    xmls_add_str(result, "graph [\n");

    template_task_graph_vertex_t *vertex = template_task_graph_vertexes(graph);
    char buffer[128];
    while (vertex != NULL)
    {
        xmls_add_str(result, "node [\n");

        memset(buffer, 0, sizeof(buffer));
        sprintf(buffer, "id %d\n", vertex->id);
        xmls_add_str(result, buffer);

        xmls_add_str(result, "]\n");
        vertex = vertex->next;
    }

    template_task_graph_edge_t *edge = graph->head;
    while (edge != NULL)
    {
        xmls_add_str(result, "edge [\n");

        memset(buffer, 0, sizeof(buffer));
        sprintf(buffer, "source %d\n", edge->source->id);
        xmls_add_str(result, buffer);

        memset(buffer, 0, sizeof(buffer));
        sprintf(buffer, "target %d\n", edge->target->id);
        xmls_add_str(result, buffer);

        memset(buffer, 0, sizeof(buffer));
        sprintf(buffer, "label %s\n", edge->label->s);
        xmls_add_str(result, buffer);

        xmls_add_str(result, "]\n");
        edge = edge->next;
    }
    xmls_add_str(result, "]\n");

    FILE *file = fopen(graph->filename->s, "w");
    if (file == NULL)
    {
        error("template_task_graph_save:: open file");
        return;
    }

    fwrite(result->s, result->len, 1, file);
    fclose(file);
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

    template_task_graph_save(graph);

    pthread_mutex_destroy(&(graph->lock));
    pthread_key_delete(graph->vertex_key);
}

void template_task_graph_set_current(template_task_graph_t *graph, template_context *task)
{
    if (graph == NULL) return;

    template_task_graph_vertex_t *v = memory_allocator_new(sizeof(template_task_graph_vertex_t *));
    v->key = template_task_graph_key(task);
    pthread_setspecific(graph->vertex_key, v);
}

void template_task_graph_add(template_task_graph_t *graph, XMLNODE *instruction, template_context *task)
{
    if (graph == NULL) return;

    template_task_graph_vertex_t *source = (template_task_graph_vertex_t *) pthread_getspecific(graph->vertex_key);
    if (source == NULL)
    {
        error("template_task_graph_add:: task not found");
        return;
    }

    template_task_graph_vertex_t *target = memory_allocator_new(sizeof(template_task_graph_vertex_t *));
    target->key = template_task_graph_key(task);

    template_task_graph_edge_t *edge = memory_allocator_new(sizeof(template_task_graph_edge_t));
    edge->label = instruction == NULL ? xsl_s_root : instruction->name;
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
