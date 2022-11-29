// #define _XOPEN_SOURCE 700
#include "scheduler.h"
#include "init.h"
#include "queue.h"
#include "sync.h"
#include "thread.h"
#include <stdlib.h>
#include <time.h>

sem_t *SEM;

void __ContextSwitch(int curpid, int newpid) {
    kill(curpid, SIGSTOP);
    kill(newpid, SIGCONT);
}

void remove_sem(int sig) {
    sem_unlink("mysem");
    sem_close(SEM);
    exit(0);
}

void scheduler(int signo, siginfo_t *info, void *context) {
    if (ReadyQueue.queueCount != 0 && pCurrentThread != NULL) {
        kill(pCurrentThread->pid, SIGSTOP);
    }
    SEM = sem_open("mysem", O_CREAT, 0644, 1);

    sem_wait(SEM);
    /*
        for (int i = 0; i < 1000000; i++) {
            int j = i;
        }
    */
    Thread *cur_thread, *new_thread;
    pid_t curpid, newpid;

    if (ReadyQueue.queueCount == 0) {
        sem_post(SEM);
        sem_close(SEM);
        sem_unlink("mysem");
        return;
    } else if (pCurrentThread != NULL) {
        // __ContextSwitch(pCurrentThread->pid, ReadyQueue.pHead->pid);
        new_thread = ReadyQueue.pHead;
        cur_thread = pCurrentThread;
        pCurrentThread = new_thread;
        new_thread->cpu_time += TIMESLICE;

        curpid = cur_thread->pid;
        newpid = new_thread->pid;

        queue_pop(&ReadyQueue);
        new_thread->status = THREAD_STATUS_RUN;

        if (cur_thread->status != THREAD_STATUS_ZOMBIE) {
            queue_push(&ReadyQueue, cur_thread);
            cur_thread->status = THREAD_STATUS_READY;
        }

    } else {
        pCurrentThread = ReadyQueue.pHead;
        pCurrentThread->cpu_time += TIMESLICE;
        queue_pop(&ReadyQueue);
        pCurrentThread->status = THREAD_STATUS_RUN;
        // kill(pCurrentThread->pid, SIGCONT);
    }

    sem_post(SEM);
    kill(pCurrentThread->pid, SIGCONT);
}

void RunScheduler(void) {
    SEM = sem_open("mysem", O_CREAT, 0644, 1);

    struct sigaction act;
    struct itimerspec value;
    timer_t timer_id = 0;

    act.sa_flags = SA_SIGINFO;
    act.sa_sigaction = scheduler;
    sigemptyset(&act.sa_mask);
    sigaction(SIGALRM, &act, NULL);
    timer_create(CLOCK_REALTIME, NULL, &timer_id);
    signal(SIGINT, remove_sem);

    value.it_interval.tv_sec = TIMESLICE;
    value.it_interval.tv_nsec = 0;
    value.it_value.tv_sec = TIMESLICE;
    value.it_value.tv_nsec = 0;

    if (pCurrentThread == NULL && ReadyQueue.queueCount != 0) {
        sem_wait(SEM);
        pCurrentThread = ReadyQueue.pHead;
        pCurrentThread->cpu_time += TIMESLICE;
        queue_pop(&ReadyQueue);
        pCurrentThread->status = THREAD_STATUS_RUN;
        kill(pCurrentThread->pid, SIGCONT);
        sem_post(SEM);
    }

    timer_settime(timer_id, 0, &value, NULL);
}
