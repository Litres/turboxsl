#include "shared_variable.h"

#include <stdlib.h>

#include "allocator.h"
#include "logger.h"

struct shared_variable_ {
    unsigned int value;
    pthread_cond_t condition;
    pthread_mutex_t mutex;
};

shared_variable *shared_variable_create()
{
    shared_variable *result = malloc(sizeof(shared_variable));

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
    free(variable);
}

void shared_variable_increase(shared_variable *variable)
{
    if (pthread_mutex_lock(&(variable->mutex)))
    {
        error("shared_variable_increase:: mutex lock");
        return;
    }

    variable->value += 1;
    pthread_mutex_unlock(&(variable->mutex));
}

void shared_variable_decrease(shared_variable *variable)
{
    if (pthread_mutex_lock(&(variable->mutex)))
    {
        error("shared_variable_decrease:: mutex lock");
        return;
    }

    variable->value -= 1;
    if (variable->value == 0)
    {
        pthread_cond_broadcast(&(variable->condition));
    }
    pthread_mutex_unlock(&(variable->mutex));
}

void shared_variable_wait(shared_variable *variable)
{
    if (pthread_mutex_lock(&(variable->mutex)))
    {
        error("shared_variable_wait:: mutex lock");
        return;
    }

    while (variable->value != 0)
    {
        if (pthread_cond_wait(&(variable->condition), &(variable->mutex)))
        {
            error("shared_variable_wait:: condition wait");
        }
    }
    pthread_mutex_unlock(&(variable->mutex));
}
