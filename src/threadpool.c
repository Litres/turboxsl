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

struct threadpool_task {
    void (*routine_cb)(void *);
    void *data;

    pthread_t signature;
    pthread_cond_t rcond;
    pthread_mutex_t rmutex;
    int terminate;
};

struct threadpool_ {
    struct threadpool_task *tasks;
    pthread_t *thr_arr;
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

    /* Obtain a task */
    if (pthread_mutex_lock(&(pool->mutex)))
    {
        error("threadpool_start_full:: mutex lock");
        return;
    }

    pthread_t sig = pthread_self();
    unsigned int i;
    for (i = 0; i < pool->num_of_threads; ++i)
    {
        if (pool->tasks[i].signature == 0)
        {
            debug("threadpool_start_full:: free task found %d (%p)", i, pool->thr_arr[i]);
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
        error("threadpool_start_full:: mutex unlock");;
		return;
	}

	if (i >= pool->num_of_threads)
    {
        debug("threadpool_start_full:: no free task");
        (*routine)(data);
    }
}


static void *worker_thr_routine(void *data)
{
    struct threadpool_task *task = (struct threadpool_task *) data;

    while (!task->terminate)
    {
        pthread_mutex_lock(&(task->rmutex));
        while (task->signature == 0)
        {
            /* wait for task */
            pthread_cond_wait(&(task->rcond), &(task->rmutex));
        }
        pthread_mutex_unlock(&(task->rmutex));

        /* Execute routine (if any). */
        if (task->routine_cb)
        {
            task->routine_cb(task->data);
        }

        task->routine_cb = NULL;
        task->signature = 0;
    }

    debug("worker_thr_routine:: terminated");
    return NULL;
}

threadpool *threadpool_init(unsigned int num_of_threads)
{
    debug("threadpool_init:: pool size %d", num_of_threads);

    if (num_of_threads == 0) return NULL;

    /* Create the thread pool struct. */
    threadpool *pool;
    if ((pool = malloc(sizeof(threadpool))) == NULL)
    {
        perror("malloc: ");
        return NULL;
    }
    pool->num_of_threads = num_of_threads;

    /* Init the mutex and cond vars. */
    if (pthread_mutex_init(&(pool->mutex), NULL))
    {
        perror("pthread_mutex_init: ");
        free(pool);
        return NULL;
    }

    /* Create the thr_arr. */
    if ((pool->thr_arr = malloc(sizeof(pthread_t) * num_of_threads)) == NULL)
    {
        perror("malloc: ");
        free(pool);
        return NULL;
    }

    /* Create the task array. */
    if ((pool->tasks = malloc(sizeof(struct threadpool_task) * num_of_threads)) == NULL)
    {
        perror("malloc: ");
        free(pool->thr_arr);
        free(pool);
        return NULL;
    }
    memset(pool->tasks, 0, sizeof(struct threadpool_task) * num_of_threads);

    /* Start the worker threads. */
    for (unsigned int i = 0; i < num_of_threads; i++)
    {
        if (pthread_mutex_init(&(pool->tasks[i].rmutex), NULL))
        {
            perror("pthread_mutex_init: ");
            free(pool);
            return NULL;
        }

        if (pthread_cond_init(&(pool->tasks[i].rcond), NULL))
        {
            perror("pthread_mutex_init: ");
            free(pool);
            return NULL;
        }

        if (pthread_create(&(pool->thr_arr[i]), NULL, worker_thr_routine, &(pool->tasks[i])))
        {
            perror("pthread_create:");
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

        pthread_join(pool->thr_arr[i], NULL);

        pthread_cond_destroy(&(pool->tasks[i].rcond));
        pthread_mutex_destroy(&(pool->tasks[i].rmutex));
    }

    free(pool->tasks);
    free(pool->thr_arr);

    pthread_mutex_destroy(&(pool->mutex));

    free(pool);
}

void threadpool_set_allocator(memory_allocator *allocator, threadpool *pool)
{
    if (!pool) return;

    debug("threadpool_set_allocator:: setup");
    for (unsigned int i = 0; i < pool->num_of_threads; i++)
    {
        memory_allocator_add_entry(allocator, pool->thr_arr[i], 10000000);
    }
}

void threadpool_set_external_cache(external_cache *cache, threadpool *pool)
{
    if (!pool) return;

    debug("threadpool_set_cache:: setup");
    for (unsigned int i = 0; i < pool->num_of_threads; i++)
    {
        external_cache_add_client(cache, pool->thr_arr[i]);
    }
}
