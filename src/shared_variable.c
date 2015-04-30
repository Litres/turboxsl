#include "shared_variable.h"

#include "allocator.h"
#include "logger.h"

struct shared_variable_ {
    unsigned int value;
    pthread_cond_t condition;
    pthread_mutex_t mutex;
};

shared_variable *shared_variable_create()
{
    shared_variable *result = memory_allocator_new(sizeof(shared_variable));

    if (pthread_mutex_init(&(result->mutex), NULL))
    {
        error("shared_variable_create:: mutex");
        return NULL;
    }
    if (pthread_cond_init(&(result->condition), NULL))
    {
        error("shared_variable_create:: condition");
        return NULL;
    }

    result->value = 1;

    return result;
}

void shared_variable_release(shared_variable *variable)
{
    pthread_cond_destroy(&(variable->condition));
    pthread_mutex_destroy(&(variable->mutex));
}

void shared_variable_increase(shared_variable *variable)
{
    pthread_mutex_lock(&(variable->mutex));
    variable->value += 1;
    pthread_mutex_unlock(&(variable->mutex));
}

void shared_variable_decrease(shared_variable *variable)
{
    pthread_mutex_lock(&(variable->mutex));
    variable->value -= 1;
    if (variable->value == 0) pthread_cond_broadcast(&(variable->condition));
    pthread_mutex_unlock(&(variable->mutex));
}

void shared_variable_wait(shared_variable *variable)
{
    pthread_mutex_lock(&(variable->mutex));
    while (variable->value != 0)
    {
        pthread_cond_wait(&(variable->condition), &(variable->mutex));
    }
    pthread_mutex_unlock(&(variable->mutex));
}
