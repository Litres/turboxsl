#ifndef UNBOUNDED_QUEUE_H_
#define UNBOUNDED_QUEUE_H_

typedef struct unbounded_queue_ unbounded_queue;

unbounded_queue *unbounded_queue_create();

void unbounded_queue_close(unbounded_queue *queue);

void unbounded_queue_release(unbounded_queue *queue);

void unbounded_queue_enqueue(unbounded_queue *queue, void *value);

void *unbounded_queue_dequeue(unbounded_queue *queue);

#endif
