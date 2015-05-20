/*
 *  TurboXSL XML+XSLT processing library
 *  Thread pool library
 *
 *
 *  (c) Egor Voznessenski, voznyak@mail.ru
 *
 *  $Id$
 *
**/
#include "threadpool.h"

#include "logger.h"
#include "unbounded_queue.h"

typedef struct threadpool_task_ {
    void (*routine)(void *);
    void *data;
} threadpool_task;

struct threadpool_ {
    pthread_t *threads;
    unsigned int num_of_threads;
    pthread_mutex_t blocked_lock;
    unsigned int num_of_blocked;
    threadpool_task *stop_task;

    unbounded_queue *queue;
};

void *worker_thr_routine(void *data)
{
    threadpool *pool = (threadpool *) data;
    for (; ;)
    {
        threadpool_task *task = unbounded_queue_dequeue(pool->queue);
        if (task == NULL || task == pool->stop_task) break;
        task->routine(task->data);
    }

    return NULL;
}

threadpool *threadpool_init(unsigned int num_of_threads)
{
    debug("threadpool_init:: pool size %d", num_of_threads);

    if (num_of_threads == 0) return NULL;

    threadpool *pool = memory_allocator_new(sizeof(threadpool));
    pool->num_of_threads = num_of_threads;
    pool->stop_task = memory_allocator_new(sizeof(threadpool_task));

    if (pthread_mutex_init(&(pool->blocked_lock), NULL))
    {
        error("shared_variable_create:: blocked lock");
        return NULL;
    }

    pool->queue = unbounded_queue_create();
    if (pool->queue == NULL)
    {
        error("threadpool_init:: queue");
        return NULL;
    }

    pool->threads = memory_allocator_new(sizeof(pthread_t) * num_of_threads);
    for (unsigned int i = 0; i < num_of_threads; i++)
    {
        if (pthread_create(&(pool->threads[i]), NULL, worker_thr_routine, pool))
        {
            error("threadpool_init:: thread");
            threadpool_free(pool);
            return NULL;
        }
    }
    return pool;
}

void threadpool_free(threadpool *pool)
{
    if (!pool) return;

    unbounded_queue_close(pool->queue);

    for (unsigned int i = 0; i < pool->num_of_threads; i++)
    {
        unbounded_queue_enqueue(pool->queue, pool->stop_task);
    }

    for (unsigned int i = 0; i < pool->num_of_threads; i++)
    {
        pthread_join(pool->threads[i], NULL);
    }

    unbounded_queue_release(pool->queue);
    pthread_mutex_destroy(&(pool->blocked_lock));
}

int thread_pool_try_wait(threadpool *pool)
{
    if (pthread_mutex_lock(&(pool->blocked_lock)))
    {
        error("thread_pool_try_wait:: lock");
        return 0;
    }

    int result = 0;
    if (pool->num_of_blocked < pool->num_of_threads)
    {
        pool->num_of_blocked += 1;
        result = 1;
    }

    pthread_mutex_unlock(&(pool->blocked_lock));

    return result;
}

void thread_pool_finish_wait(threadpool *pool)
{
    if (pthread_mutex_lock(&(pool->blocked_lock)))
    {
        error("thread_pool_finish_wait:: lock");
        return;
    }

    if (pool->num_of_blocked > 0) pool->num_of_blocked -= 1;

    pthread_mutex_unlock(&(pool->blocked_lock));
}

void threadpool_start(threadpool *pool, void (*routine)(void *), void *data)
{
    if (!pool)
    {
        (*routine)(data);
        return;
    }

    threadpool_task *task = memory_allocator_new(sizeof(threadpool_task));
    task->routine = routine;
    task->data = data;
    unbounded_queue_enqueue(pool->queue, task);
}

void threadpool_set_allocator(memory_allocator *allocator, threadpool *pool)
{
    if (!pool) return;

    debug("threadpool_set_allocator:: setup");
    for (unsigned int i = 0; i < pool->num_of_threads; i++)
    {
        memory_allocator_add_entry(allocator, pool->threads[i], 1000000);
    }
}

void threadpool_set_external_cache(external_cache *cache, threadpool *pool)
{
    if (!pool) return;

    debug("threadpool_set_external_cache:: setup");
    for (unsigned int i = 0; i < pool->num_of_threads; i++)
    {
        external_cache_add_client(cache, pool->threads[i]);
    }
}
