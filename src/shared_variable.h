#ifndef SHARED_VARIABLE_H_
#define SHARED_VARIABLE_H_

typedef struct shared_variable_ shared_variable;

shared_variable *shared_variable_create();

void shared_variable_release(shared_variable *variable);

void shared_variable_increase(shared_variable *variable);

void shared_variable_decrease(shared_variable *variable);

void shared_variable_wait(shared_variable *variable);

#endif
