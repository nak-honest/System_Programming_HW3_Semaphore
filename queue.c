#include "queue.h"
#include "stdio.h"

/* Queue의 tail에 스레드 추가 */
void queue_push(ThreadQueue *q, Thread *t) {
    if (q->queueCount == 0) {
        t->pNext = NULL;
        t->pPrev = NULL;
        q->pHead = t;
        q->pTail = t;
    } else {
        t->pNext = NULL;
        t->pPrev = q->pTail;
        q->pTail->pNext = t;
        q->pTail = t;
    }
    q->queueCount++;
}

/* Queue의 head의 스레드 삭제 */
void queue_pop(ThreadQueue *q) {
    if (q->queueCount == 0) {
        perror("queue_pop() : queue is empty, underflow!");
        return;
    } else if (q->queueCount == 1) {
        q->pHead = NULL;
        q->pTail = NULL;
    } else {
        Thread *remove_thread;
        remove_thread = q->pHead;
        q->pHead = remove_thread->pNext;
        remove_thread->pNext = NULL;
    }
    q->queueCount--;
}

/* Queue에서 스레드를 찾아서 삭제 */
void queue_remove(ThreadQueue *q, Thread *t) {
    Thread *cusor;
    for (cusor = q->pHead; cusor != NULL; cusor = cusor->pNext) {
        if (cusor == t) {
            break;
        }
    }

    if (q->queueCount == 0) {
        perror("queue_remove() : This queue is empty, underflow!");
        return;
    } else if (cusor == NULL) {
        perror("queue_remove() : This thread is not in queue!");
        return;
    } else if (q->queueCount == 1) {
        q->pHead = NULL;
        q->pTail = NULL;
        q->queueCount--;
    } else {
        if (q->pHead == t) {
            queue_pop(q);
        } else if (q->pTail == t) {
            q->pTail = t->pPrev;
            t->pPrev = NULL;
            q->queueCount--;
        } else {
            t->pPrev->pNext = t->pNext;
            t->pNext->pPrev = t->pPrev;
            t->pPrev = NULL;
            t->pNext = NULL;
            q->queueCount--;
        }
    }
}
