#ifndef __QUEUE_H__
#define __QUEUE_H__

#include "thread.h"

void queue_push(ThreadQueue *q, Thread *t);
void queue_pop(ThreadQueue *q);
void queue_remove(ThreadQueue *q, Thread *t);

#endif // __QUEUE_H__
