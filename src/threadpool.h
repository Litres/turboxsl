/**
 * threadpool.h
 *
 */

#ifndef THREADPOOL_H_
#define THREADPOOL_H_

#include "allocator.h"
#include "external_cache.h"

typedef struct threadpool_ threadpool;

/**
 * This function creates a newly allocated thread pool.
 *
 * @param num_of_threads The number of worker thread used in this pool.
 * @return On success returns a newly allocated thread pool, on failure NULL is returned.
 */
threadpool* threadpool_init(unsigned int num_of_threads);

void threadpool_free(threadpool *pool);

int thread_pool_try_wait(threadpool *pool);
void thread_pool_finish_wait(threadpool *pool);

void threadpool_start(threadpool *pool, void (*routine)(void *), void *data);

void threadpool_set_allocator(memory_allocator *allocator, threadpool *pool);
void threadpool_set_external_cache(external_cache *cache, threadpool *pool);

#endif /* THREADPOOL_H_ */
