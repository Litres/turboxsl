/**
 * threadpool.h
 *
 */

#ifndef THREADPOOL_H_
#define THREADPOOL_H_

#include "allocator.h"
#include "external_cache.h"

struct threadpool;

/**
 * This function creates a newly allocated thread pool.
 *
 * @param num_of_threads The number of worker thread used in this pool.
 * @return On success returns a newly allocated thread pool, on failure NULL is returned.
 */
struct threadpool* threadpool_init(int num_of_threads);

void threadpool_free(struct threadpool *pool);

/**
 * This function waits for children to finish
 *
 * @param pool The thread pool structure.
 * @param signature Signature of the parent thread
 *
 */
void threadpool_wait(struct threadpool *pool);

void threadpool_free(struct threadpool *pool);

void threadpool_set_allocator(memory_allocator *allocator, struct threadpool *pool);
void threadpool_set_external_cache(external_cache *cache, struct threadpool *pool);

#endif /* THREADPOOL_H_ */
