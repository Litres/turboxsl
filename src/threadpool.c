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

#include <stdlib.h>
#include <string.h>

#include "logger.h"

typedef struct threadpool_task_ {
    void (*routine_cb)(void *);
    void *data;

    pthread_t signature;
    pthread_cond_t rcond;
    pthread_mutex_t rmutex;
    int terminate;
} threadpool_task;

struct threadpool_ {
    threadpool_task *tasks;
    pthread_t *threads;
    unsigned int num_of_threads;

    pthread_mutex_t mutex;
};

void threadpool_wait(threadpool *pool)
{
    if (!pool) return;

    debug("threadpool_wait:: waiting");
    for (; ;)
    {
        int n = 0;
        for (int i = 0; i < pool->num_of_threads; ++i)
        {
            if (pool->tasks[i].signature) ++n;
        }

        if (n == 0) break;

        struct timespec polling_interval;
        polling_interval.tv_sec = 0;
        polling_interval.tv_nsec = 1000000;
        nanosleep(&polling_interval, NULL);
    }
}

void threadpool_start(threadpool *pool, void (*routine)(void *), void *data)
{
    if (!pool)
    {
        (*routine)(data);
        return;
    }

    if (pthread_mutex_lock(&(pool->mutex)))
    {
        error("threadpool_start:: mutex lock");
        return;
    }

    pthread_t sig = pthread_self();
    unsigned int i;
    for (i = 0; i < pool->num_of_threads; ++i)
    {
        if (pool->tasks[i].signature == 0)
        {
            pool->tasks[i].routine_cb = routine;
            pool->tasks[i].data = data;
            pool->tasks[i].signature = sig;

            pthread_mutex_lock(&(pool->tasks[i].rmutex));
            pthread_cond_broadcast(&(pool->tasks[i].rcond));
            pthread_mutex_unlock(&(pool->tasks[i].rmutex));

            break;
        }
    }

	if (pthread_mutex_unlock(&(pool->mutex)))
    {
        error("threadpool_start:: mutex unlock");;
		return;
	}

	if (i >= pool->num_of_threads) (*routine)(data);
}

static void *worker_thr_routine(void *data)
{
    threadpool_task *task = (threadpool_task *) data;

    while (!task->terminate)
    {
        pthread_mutex_lock(&(task->rmutex));
        while (task->signature == 0) pthread_cond_wait(&(task->rcond), &(task->rmutex));
        pthread_mutex_unlock(&(task->rmutex));

        if (task->routine_cb) task->routine_cb(task->data);

        task->routine_cb = NULL;
        task->data = NULL;
        task->signature = 0;
    }

    return NULL;
}

threadpool *threadpool_init(unsigned int num_of_threads)
{
    debug("threadpool_init:: pool size %d", num_of_threads);

    if (num_of_threads == 0) return NULL;

    threadpool *pool = malloc(sizeof(threadpool));
    if (pool == NULL)
    {
        error("threadpool_init:: malloc");
        return NULL;
    }
    pool->num_of_threads = num_of_threads;

    if (pthread_mutex_init(&(pool->mutex), NULL))
    {
        error("threadpool_init:: mutex");
        free(pool);
        return NULL;
    }

    pool->threads = malloc(sizeof(pthread_t) * num_of_threads);
    if (pool->threads == NULL)
    {
        error("threadpool_init:: malloc");
        free(pool);
        return NULL;
    }

    pool->tasks = malloc(sizeof(threadpool_task) * num_of_threads);
    if (pool->tasks == NULL)
    {
        error("threadpool_init:: malloc");
        free(pool->threads);
        free(pool);
        return NULL;
    }
    memset(pool->tasks, 0, sizeof(threadpool_task) * num_of_threads);

    for (unsigned int i = 0; i < num_of_threads; i++)
    {
        if (pthread_mutex_init(&(pool->tasks[i].rmutex), NULL))
        {
            error("threadpool_init:: mutex");
            free(pool);
            return NULL;
        }

        if (pthread_cond_init(&(pool->tasks[i].rcond), NULL))
        {
            error("threadpool_init:: variable");
            free(pool);
            return NULL;
        }

        if (pthread_create(&(pool->threads[i]), NULL, worker_thr_routine, &(pool->tasks[i])))
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

    pthread_t self = pthread_self();
    for (unsigned int i = 0; i < pool->num_of_threads; i++)
    {
        debug("threadpool_free:: task %d", i);
        pool->tasks[i].terminate = 1;
        pool->tasks[i].signature = self;

        pthread_mutex_lock(&(pool->tasks[i].rmutex));
        pthread_cond_broadcast(&(pool->tasks[i].rcond));
        pthread_mutex_unlock(&(pool->tasks[i].rmutex));

        pthread_join(pool->threads[i], NULL);

        pthread_cond_destroy(&(pool->tasks[i].rcond));
        pthread_mutex_destroy(&(pool->tasks[i].rmutex));
    }

    free(pool->tasks);
    free(pool->threads);

    pthread_mutex_destroy(&(pool->mutex));

    free(pool);
}

void threadpool_set_allocator(memory_allocator *allocator, threadpool *pool)
{
    if (!pool) return;

    debug("threadpool_set_allocator:: setup");
    for (unsigned int i = 0; i < pool->num_of_threads; i++)
    {
        memory_allocator_add_entry(allocator, pool->threads[i], 500000);
    }
}

void threadpool_set_external_cache(external_cache *cache, threadpool *pool)
{
    if (!pool) return;

    debug("threadpool_set_cache:: setup");
    for (unsigned int i = 0; i < pool->num_of_threads; i++)
    {
        external_cache_add_client(cache, pool->threads[i]);
    }
}
