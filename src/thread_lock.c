#include "thread_lock.h"

#include "logger.h"

int thread_lock_create_recursive(pthread_mutex_t *lock)
{
    pthread_mutexattr_t attribute;
    if (pthread_mutexattr_init(&attribute))
    {
        error("thread_lock_create_recursive:: create lock");
        return 0;
    }

    pthread_mutexattr_settype(&attribute, PTHREAD_MUTEX_RECURSIVE);
    if (pthread_mutex_init(lock, &attribute))
    {
        error("thread_lock_create_recursive:: create lock");
        return 0;
    }
    pthread_mutexattr_destroy(&attribute);

    return 1;
}
