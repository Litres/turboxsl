#include "template_task.h"

void f2(template_context *context)
{
    printf("f2: %p\n", pthread_self());
}

void f1(template_context *context)
{
    printf("f1: %p\n", pthread_self());
    for (int i = 0; i < 5; i++)
    {
        template_context *new_context = memory_allocator_new(sizeof(template_context));
        new_context->context = context->context;
        new_context->workers = context->workers;
        template_task_run(new_context, f2);
    }
}

int main(int argc, char *argv[])
{
    memory_allocator *allocator = memory_allocator_create(NULL);
    memory_allocator_add_entry(allocator, pthread_self(), 1000);
    memory_allocator_set_current(allocator);

    threadpool *pool = threadpool_init(4);
    threadpool_set_allocator(allocator, pool);

    TRANSFORM_CONTEXT *transform_context = memory_allocator_new(sizeof(TRANSFORM_CONTEXT));
    transform_context->pool = pool;

    printf("starting task\n");

    template_context *context = memory_allocator_new(sizeof(template_context));
    context->context = transform_context;
    template_task_run_and_wait(context, f1);

    printf("task completed\n");

    threadpool_free(pool);
    memory_allocator_release(allocator);

    return 0;
}