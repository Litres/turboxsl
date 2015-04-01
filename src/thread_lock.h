#ifndef THREAD_LOCK_H_
#define THREAD_LOCK_H_

#include <pthread.h>

int thread_lock_create_recursive(pthread_mutex_t *lock);

#endif
