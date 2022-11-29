// #define _XOPEN_SOURCE 700
#include "thread.h"
#include "init.h"
#include "queue.h"
#include "scheduler.h"
#include "sync.h"
#include <linux/sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define STACK_SIZE 1024 * 64

ThreadQueue ReadyQueue;
ThreadQueue WaitingQueue;
ThreadTblEnt pThreadTbEnt[MAX_THREAD_NUM];
Thread *pCurrentThread; // Running 상태의 Thread를 가리키는 변수

void Init(void) {
    /* ReadyQueue 초기화 */
    ReadyQueue.queueCount = 0;
    ReadyQueue.pHead = NULL;
    ReadyQueue.pTail = NULL;

    /* WaitingQueue 초기화 */
    WaitingQueue.queueCount = 0;
    WaitingQueue.pHead = NULL;
    WaitingQueue.pTail = NULL;

    pCurrentThread = NULL; // pCurrentThread 초기화

    /* pThreadTbEnt 멤버 초기화 */
    for (int i = 0; i < MAX_THREAD_NUM; i++) {
        pThreadTbEnt[i].bUsed = 0;
        pThreadTbEnt[i].pThread = NULL;
    }
}
int thread_create(thread_t *thread, thread_attr_t *attr,
                  void *(*start_routine)(void *), void *arg) {
    Thread *new_thread;
    void *stack;
    BOOL is_full = 1;
    int tid;
    pid_t new_pid;

    for (int i = 0; i < MAX_THREAD_NUM; i++) {
        if (pThreadTbEnt[i].bUsed == 0) {
            is_full = 0;
            tid = i;
            break;
        }
    }

    if (is_full == 1) {
        perror("The number of creatable threads are full!");
        sem_post(SEM);
        return -1;
    }

    new_thread = (Thread *)malloc(sizeof(Thread));
    stack = malloc(STACK_SIZE);
    new_pid = clone(start_routine, (char *)stack + STACK_SIZE,
                    CLONE_VM | CLONE_SIGHAND | CLONE_FS | CLONE_FILES, arg);

    kill(new_pid, SIGSTOP);
    *thread = tid;

    new_thread->stackSize = STACK_SIZE;
    new_thread->stackAddr = stack;
    new_thread->status = THREAD_STATUS_READY;
    new_thread->pid = new_pid;
    new_thread->cpu_time = 0;

    queue_push(&ReadyQueue, new_thread);
    pThreadTbEnt[tid].pThread = new_thread;
    pThreadTbEnt[tid].bUsed = 1;

    return 0;
}

int thread_suspend(thread_t tid) {
    SEM = sem_open("mysem", O_CREAT, 0644, 1);
    sem_wait(SEM);
    if (pThreadTbEnt[tid].bUsed == 0) {
        perror("thread_suspend() : That tid is not exist!");
        sem_post(SEM);
        return -1;
    }

    Thread *thread = pThreadTbEnt[tid].pThread;

    if (pCurrentThread == thread) {
        perror("thread_suspend() : self-suspend!");
        sem_post(SEM);
        return -1;
    }

    if (thread->status == THREAD_STATUS_READY) {
        queue_remove(&ReadyQueue, thread);
        thread->status = THREAD_STATUS_WAIT;
        queue_push(&WaitingQueue, thread);
    }

    sem_post(SEM);
    return 0;
}

int thread_cancel(thread_t tid) {
    SEM = sem_open("mysem", O_CREAT, 0644, 1);
    sem_wait(SEM);
    if (pThreadTbEnt[tid].bUsed == 0) {
        perror("thread_resume() : That tid is not exist!");
        sem_post(SEM);
        return -1;
    }

    Thread *thread = pThreadTbEnt[tid].pThread;
    pid_t tpid = thread->pid;

    if (pCurrentThread == thread) {
        perror("thread_cancel() : self-cancel!");
        sem_post(SEM);

        return -1;
    }

    kill(tpid, SIGKILL);

    if (thread->status == THREAD_STATUS_READY) {
        queue_remove(&ReadyQueue, thread);
    } else if (thread->status == THREAD_STATUS_WAIT) {
        queue_remove(&WaitingQueue, thread);
    }

    pThreadTbEnt[tid].bUsed = 0;
    pThreadTbEnt[tid].pThread = NULL;

    free(thread->stackAddr);
    free(thread);

    sem_post(SEM);
    return 0;
}

int thread_resume(thread_t tid) {
    SEM = sem_open("mysem", O_CREAT, 0644, 1);
    sem_wait(SEM);
    if (pThreadTbEnt[tid].bUsed == 0) {
        perror("thread_resume() : That tid is not exist!");
        sem_post(SEM);
        return -1;
    }

    Thread *thread = pThreadTbEnt[tid].pThread;

    if (thread->status == THREAD_STATUS_WAIT) {
        queue_remove(&WaitingQueue, thread);
        thread->status = THREAD_STATUS_READY;
        queue_push(&ReadyQueue, thread);
    }

    sem_post(SEM);
    return 0;
}

thread_t thread_self(void) {
    SEM = sem_open("mysem", O_CREAT, 0644, 1);
    sem_wait(SEM);
    thread_t tid;
    for (int i = 0; i < MAX_THREAD_NUM; i++) {
        if (pThreadTbEnt[i].pThread == pCurrentThread) {
            tid = i;
        }
    }

    sem_post(SEM);
    return tid;
}

void disjoin(int signo) {}

int thread_join(thread_t tid) {
    SEM = sem_open("mysem", O_CREAT, 0644, 1);
    sem_wait(SEM);

    Thread *new_thread, *parent_thread, *child_thread;
    pid_t curpid, newpid;

    parent_thread = pCurrentThread;
    child_thread = pThreadTbEnt[tid].pThread;

    signal(SIGCHLD, disjoin);

    pCurrentThread->status = THREAD_STATUS_WAIT;
    queue_push(&WaitingQueue, parent_thread);

    if (ReadyQueue.queueCount != 0) {
        new_thread = ReadyQueue.pHead;
        queue_pop(&ReadyQueue);
        new_thread->status = THREAD_STATUS_RUN;
        new_thread->cpu_time += 2;
        newpid = new_thread->pid;
        kill(newpid, SIGCONT);
        pCurrentThread = new_thread;
    } else {
        pCurrentThread = NULL;
    }

    sem_post(SEM);

    while (child_thread->status != THREAD_STATUS_ZOMBIE) {
        pause();
    }

    sem_wait(SEM);

    if (pCurrentThread == child_thread) {
        if (ReadyQueue.queueCount != 0) {
            new_thread = ReadyQueue.pHead;
            queue_pop(&ReadyQueue);
            new_thread->status = THREAD_STATUS_RUN;
            new_thread->cpu_time += 2;
            newpid = new_thread->pid;
            kill(newpid, SIGCONT);
            pCurrentThread = new_thread;
        } else {
            pCurrentThread = NULL;
        }
    }

    free(child_thread->stackAddr);
    child_thread->stackAddr = NULL;
    free(child_thread);
    child_thread = NULL;

    pThreadTbEnt[tid].bUsed = 0;
    pThreadTbEnt[tid].pThread = NULL;

    queue_remove(&WaitingQueue, parent_thread);

    if (ReadyQueue.queueCount == 0 && pCurrentThread == NULL) {
        pCurrentThread = parent_thread;
        parent_thread->status = THREAD_STATUS_RUN;
        parent_thread->cpu_time += TIMESLICE; // 여기 다시 확인
    } else {
        parent_thread->status = THREAD_STATUS_READY;
        queue_push(&ReadyQueue, parent_thread);
    }

    sem_post(SEM);

    if (ReadyQueue.queueCount != 0) {
        kill(parent_thread->pid, SIGSTOP);
    }

    return 0;
}

int thread_cputime(void) {
    SEM = sem_open("mysem", O_CREAT, 0644, 1);
    sem_wait(SEM);
    int time = (int)pCurrentThread->cpu_time;

    sem_post(SEM);
    return time;
}

void thread_exit(void) { pCurrentThread->status = THREAD_STATUS_ZOMBIE; }
