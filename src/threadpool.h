/**
 * threadpool.h
 *
 */

#ifndef THREADPOOL_H_
#define THREADPOOL_H_

struct threadpool;


/**
 * This function creates a newly allocated thread pool.
 *
 * @param num_of_threads The number of worker thread used in this pool.
 * @return On success returns a newly allocated thread pool, on failure NULL is returned.
 */
struct threadpool* threadpool_init(int num_of_threads);

/**
 * This function adds a routine to be exexuted by the threadpool at some future time.
 *
 * @param pool The thread pool structure.
 * @param routine The routine to be executed.
 * @param data The data to be passed to the routine.
 *
 * @return >=0 on success (index of thread in the pool).
 * @return -1 on failure.
 * @return -2 if pool is full.
 */
int threadpool_start(struct threadpool *pool, void (*routine)(void*), void *data);
int threadpool_start_rec(struct threadpool *pool, void (*routine)(void*), void *data, int depth);

/**
 * This function returns running status of the worker thread
 *
 * @param pool The thread pool structure.
 * @param nthread Thread index returned by threadpool_start
 *
 * @return 1 if running
 * @return 0 if completed
 * @return -1 if error (eg no such thread)
 */
int threadpool_is_running(struct threadpool *pool, int nthread);

/**
 * This function returns number of free threads available in the pool
 *
 * @param pool The thread pool structure.
 *
 * @return Number of threads available for running
 */
int threadpool_available_threads(struct threadpool *pool);

/**
 * This function returns number of free threads available in the pool
 *
 * @param pool The thread pool structure.
 * @param signature Signature of the parent thread
 *
 * @return Number of child threads running (0 if all finished)
 */
int threadpool_busy_threads(struct threadpool *pool);

/**
 * This function waits for children to finish
 *
 * @param pool The thread pool structure.
 * @param signature Signature of the parent thread
 *
 */
void threadpool_wait(struct threadpool *pool);

void threadpool_free(struct threadpool *pool);

void threadpool_lock(struct threadpool *pool);

void threadpool_unlock(struct threadpool *pool);

int threadpool_id(struct threadpool *pool);

int threadpool_ready_threads(struct threadpool *pool);

#endif /* THREADPOOL_H_ */
