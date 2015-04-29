#include "unbounded_queue.h"

#include "thread_lock.h"
#include "logger.h"
#include "allocator.h"

typedef struct unbounded_queue_entry_ {
    void *value;
    struct unbounded_queue_entry_ *next;
} unbounded_queue_entry;

struct unbounded_queue_ {
    pthread_mutex_t write_lock;
    pthread_mutex_t read_lock;
    pthread_cond_t read_condition;
    unbounded_queue_entry *head;
    unbounded_queue_entry *tail;
    int is_closed;
};

unbounded_queue *unbounded_queue_create()
{
    unbounded_queue *result = memory_allocator_new(sizeof(unbounded_queue));
    if (result == NULL)
    {
        error("unbounded_queue_create:: memory");
        return NULL;
    }

    result->head = memory_allocator_new(sizeof(unbounded_queue_entry));
    if (result->head == NULL)
    {
        error("unbounded_queue_create:: memory");
        return NULL;
    }
    result->tail = result->head;

    thread_lock_create_recursive(&(result->write_lock));
    thread_lock_create_recursive(&(result->read_lock));
    if (pthread_cond_init(&(result->read_condition), NULL))
    {
        error("unbounded_queue_create:: read condition");
        return NULL;
    }

    return result;
}

void unbounded_queue_close(unbounded_queue *queue)
{
    if (pthread_mutex_lock(&(queue->read_lock)))
    {
        error("unbounded_queue_dequeue:: lock");
        return;
    }

    queue->is_closed = 1;

    pthread_mutex_unlock(&(queue->read_lock));
}

void unbounded_queue_release(unbounded_queue *queue)
{
    pthread_mutex_destroy(&(queue->write_lock));
    pthread_mutex_destroy(&(queue->read_lock));
    pthread_cond_destroy(&(queue->read_condition));
}

void unbounded_queue_enqueue(unbounded_queue *queue, void *value)
{
    if (pthread_mutex_lock(&(queue->write_lock)))
    {
        error("unbounded_queue_enqueue:: write lock");
        return;
    }

    unbounded_queue_entry *entry = memory_allocator_new(sizeof(unbounded_queue_entry));
    if (entry == NULL)
    {
        error("unbounded_queue_enqueue:: memory");
    }
    else
    {
        entry->value = value;
        queue->tail->next = entry;
        queue->tail = entry;
    }

    pthread_mutex_unlock(&(queue->write_lock));

    if (pthread_mutex_lock(&(queue->read_lock)))
    {
        error("unbounded_queue_enqueue:: read lock");
        return;
    }
    pthread_cond_broadcast(&(queue->read_condition));
    pthread_mutex_unlock(&(queue->read_lock));
}

void *unbounded_queue_dequeue(unbounded_queue *queue)
{
    if (pthread_mutex_lock(&(queue->read_lock)))
    {
        error("unbounded_queue_dequeue:: lock");
        return NULL;
    }

    if (queue->is_closed)
    {
        pthread_mutex_unlock(&(queue->read_lock));
        return NULL;
    }

    while (queue->head->next == NULL) pthread_cond_wait(&(queue->read_condition), &(queue->read_lock));

    void *result = queue->head->next->value;
    queue->head = queue->head->next;

    pthread_mutex_unlock(&(queue->read_lock));

    return result;
}
