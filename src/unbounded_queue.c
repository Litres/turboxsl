#include "unbounded_queue.h"

#include <stdlib.h>
#include <string.h>

#include "thread_lock.h"
#include "logger.h"

typedef struct unbounded_queue_entry_ {
    void *value;
    struct unbounded_queue_entry_ *next;
} unbounded_queue_entry;

struct unbounded_queue_ {
    pthread_mutex_t lock;
    pthread_cond_t condition;
    unbounded_queue_entry *head;
    unbounded_queue_entry *tail;
    int is_closed;
};

unbounded_queue *unbounded_queue_create()
{
    unbounded_queue *result = malloc(sizeof(unbounded_queue));
    if (result == NULL)
    {
        error("unbounded_queue_create:: memory");
        return NULL;
    }

    memset(result, 0, sizeof(unbounded_queue));

    result->head = malloc(sizeof(unbounded_queue_entry));
    if (result->head == NULL)
    {
        error("unbounded_queue_create:: memory");
        return NULL;
    }

    result->head->value = NULL;
    result->head->next = NULL;
    result->tail = result->head;

    thread_lock_create_recursive(&(result->lock));
    if (pthread_cond_init(&(result->condition), NULL))
    {
        error("unbounded_queue_create:: condition");
        return NULL;
    }

    return result;
}

void unbounded_queue_close(unbounded_queue *queue)
{
    if (pthread_mutex_lock(&(queue->lock)))
    {
        error("unbounded_queue_close:: lock");
        return;
    }

    queue->is_closed = 1;

    pthread_mutex_unlock(&(queue->lock));
}

void unbounded_queue_release(unbounded_queue *queue)
{
    pthread_mutex_destroy(&(queue->lock));
    pthread_cond_destroy(&(queue->condition));

    free(queue);
}

void unbounded_queue_enqueue(unbounded_queue *queue, void *value)
{
    if (pthread_mutex_lock(&(queue->lock)))
    {
        error("unbounded_queue_enqueue:: lock");
        return;
    }

    unbounded_queue_entry *entry = malloc(sizeof(unbounded_queue_entry));
    if (entry == NULL)
    {
        error("unbounded_queue_enqueue:: memory");
    }
    else
    {
        entry->value = value;
        entry->next = NULL;

        queue->tail->next = entry;
        queue->tail = entry;
    }

    pthread_cond_broadcast(&(queue->condition));
    pthread_mutex_unlock(&(queue->lock));
}

void *unbounded_queue_dequeue(unbounded_queue *queue)
{
    if (pthread_mutex_lock(&(queue->lock)))
    {
        error("unbounded_queue_dequeue:: lock");
        return NULL;
    }

    if (queue->is_closed)
    {
        pthread_mutex_unlock(&(queue->lock));
        return NULL;
    }

    while (queue->head->next == NULL)
    {
        pthread_cond_wait(&(queue->condition), &(queue->lock));
    }

    void *result = queue->head->next->value;
    unbounded_queue_entry *head = queue->head;
    queue->head = queue->head->next;
    free(head);

    pthread_mutex_unlock(&(queue->lock));

    return result;
}
