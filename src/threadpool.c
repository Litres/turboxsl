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


#include "ltr_xsl.h"
#include <stdlib.h>
#include <pthread.h>
#include <stdio.h>
#include <signal.h>

struct threadpool;

struct threadpool_task
{
	void (*routine_cb)();
  TRANSFORM_CONTEXT *pctx;
  XMLNODE *ret;
  XMLNODE *source;
  XMLNODE *params;
  XMLNODE *locals;
  void *mode;
  pthread_t signature;
  struct threadpool *pool;
  pthread_mutex_t ownMutex;
	pthread_cond_t rcond;
	pthread_mutex_t rmutex;
};

struct threadpool
{
	struct threadpool_task *tasks;
	pthread_t *thr_arr;

	unsigned short num_of_threads;
	volatile unsigned short stop_flag;

  pthread_t main_thread;
	pthread_mutex_t mutex;
	pthread_mutex_t lock;
  pthread_mutexattr_t attr;
	pthread_mutex_t block;

  unsigned nblocks;
};

int threadpool_ready_threads(struct threadpool *pool)
{
int i;
int c=0;
  if(!pool)return 0;

  for(i=0; i<pool->num_of_threads;++i)
    if(pool->tasks[i].signature==0)
      ++c;

  return c;
}

int threadpool_id(struct threadpool *pool)
{
int i;
pthread_t p = pthread_self();

  if(!pool)return -1;

  for(i=0; i<pool->num_of_threads;++i)
    if(pool->thr_arr[i]==p)
      return i;

  return -1;
}

void threadpool_wait(struct threadpool *pool)
{
  int i,n;
  pthread_t signature;
  if(!pool)return;
  signature = pthread_self();

  for(;;)
  {
//    pthread_mutex_lock(&(pool->mutex));
    for(n=i=0;i<pool->num_of_threads;++i) {
      if(pool->tasks[i].signature)
        ++n;
    }
//    pthread_mutex_unlock(&(pool->mutex));
    if(n == 0)break;
//    pthread_yield();
  }
}

int threadpool_busy_threads(struct threadpool *pool)
{
  int i;
  int n = 0;
  pthread_t signature;
  if(!pool)return 0;
  signature = pthread_self();
  for(i=0;i<pool->num_of_threads;++i) {
    if(pool->tasks[i].signature == signature)
      ++n;
  }
  return n;
}

void threadpool_lock(struct threadpool *pool)
{
  if(!pool)return;
  pthread_mutex_lock(&(pool->lock));
}

void threadpool_unlock(struct threadpool *pool)
{
  if(!pool)return;
  pthread_mutex_unlock(&(pool->lock));
}

int threadpool_start(struct threadpool *pool, void (*routine)(void*), void *data)
{
}

void threadpool_start_full(void (*routine)(TRANSFORM_CONTEXT *, XMLNODE *, XMLNODE *, XMLNODE *, XMLNODE *, void *), TRANSFORM_CONTEXT *pctx, XMLNODE *ret, XMLNODE *source, XMLNODE *params, XMLNODE *locals, void *mode)
{
  int i,j;
  pthread_t sig = pthread_self();
  struct threadpool *pool = pctx->gctx->pool;

  if(1||!pool){
    (*routine)(pctx,ret,source,params,locals,mode);
    //return -2;
    return;
  }
	/* Obtain a task */
	if (pthread_mutex_lock(&(pool->mutex))) {
		perror("pthread_mutex_lock: ");
		//return -1;
		return;
	}

  for(i=0;i<pool->num_of_threads;++i) {
    if(pool->tasks[i].signature == 0) {
      pool->tasks[i].pctx = pctx;
      pool->tasks[i].ret = ret;
      pool->tasks[i].source = source;
      pool->tasks[i].params = params;
      pool->tasks[i].locals = locals;
      pool->tasks[i].mode = mode;

      pool->tasks[i].routine_cb = routine;
      pool->tasks[i].signature = sig;

fprintf(stderr,"++++ try start %p\n",&(pool->tasks[i]));

      pthread_mutex_lock(&(pool->tasks[i].rmutex));
      pthread_cond_broadcast(&(pool->tasks[i].rcond));
      pthread_mutex_unlock(&(pool->tasks[i].rmutex));
fprintf(stderr,"++++ broadcast %p\n",&(pool->tasks[i]));
      break;
    }
  }

	if (pthread_mutex_unlock(&(pool->mutex))) {
		perror("pthread_mutex_unlock: ");
		//return -1;
		return;
	}

	/*
	  if(i >= pool->num_of_threads) {
	    (*routine)(pctx,ret,source,params,locals,mode);
	    return -2;
	  }

	  return i;
  */
	
	if (i >= pool->num_of_threads)
		(*routine)(pctx,ret,source,params,locals,mode);

  return;
}


static void *worker_thr_routine(void *data)
{
	struct threadpool_task *task = (struct threadpool_task*)data;
  int i;

	while (1) {
		pthread_mutex_lock(&(task->rmutex));
    while(task->signature == 0) { /* wait for task */
    pthread_cond_wait(&(task->rcond), &(task->rmutex));
    }
		pthread_mutex_unlock(&(task->rmutex));
  
    fprintf(stderr,"--started %p\n",data);
  	/* Execute routine (if any). */
		if (task->routine_cb) {
			task->routine_cb(task->pctx,task->ret,task->source,task->params,task->locals,task->mode);
    }
    fprintf(stderr,"--ended %p\n",data);
    task->signature = 0;
	}

	return NULL;
}

struct threadpool* threadpool_init(int num_of_threads)
{
	struct threadpool *pool;
	int i;

	/* Create the thread pool struct. */
	if ((pool = malloc(sizeof(struct threadpool))) == NULL) {
		perror("malloc: ");
		return NULL;
	}

	pool->stop_flag = 0;
  pool->main_thread = pthread_self();

	/* Init the mutex and cond vars. */
	if (pthread_mutex_init(&(pool->mutex),NULL)) {
		perror("pthread_mutex_init: ");
		free(pool);
		return NULL;
	}
	if (pthread_mutex_init(&(pool->lock),NULL)) {
		perror("pthread_mutex_init: ");
		free(pool);
		return NULL;
	}
	if (pthread_mutex_init(&(pool->block),NULL)) {
		perror("pthread_mutex_init: ");
		free(pool);
		return NULL;
	}

	/* Create the thr_arr. */
	if ((pool->thr_arr = malloc(sizeof(pthread_t) * num_of_threads)) == NULL) {
		perror("malloc: ");
		free(pool);
		return NULL;
	}

	/* Create the task array. */
	if ((pool->tasks = malloc(sizeof(struct threadpool_task) * num_of_threads)) == NULL) {
		perror("malloc: ");
    free(pool->thr_arr);
		free(pool);
		return NULL;
	}


	/* Start the worker threads. */
	for (pool->num_of_threads = 0; pool->num_of_threads < num_of_threads; (pool->num_of_threads)++) {
	  if (pthread_mutex_init(&(pool->tasks[pool->num_of_threads].rmutex),NULL)) {
		  perror("pthread_mutex_init: ");
		  free(pool);
		  return NULL;
	  }

	    if (pthread_cond_init(&(pool->tasks[pool->num_of_threads].rcond),NULL)) {
		  perror("pthread_mutex_init: ");
		  free(pool);
		  return NULL;
	  }

	  if (pthread_mutex_init(&(pool->tasks[pool->num_of_threads].ownMutex), NULL)) {
		  perror("thread_own_mutex_init: ");
		  free(pool);
		  return NULL;
	  }

    pool->tasks[pool->num_of_threads].pool = pool;
    pool->tasks[pool->num_of_threads].signature = 0;

		if (pthread_create(&(pool->thr_arr[pool->num_of_threads]),NULL,worker_thr_routine,&(pool->tasks[pool->num_of_threads]))) {
			perror("pthread_create:");

			threadpool_free(pool);

			return NULL;
		}
	}
	return pool;
}

unsigned long threadpool_get_signature()
{
static unsigned long s = 0;
  s += 2;
  return s|1;
}

void threadpool_free(struct threadpool *pool)
{
}

