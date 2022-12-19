// #define _XOPEN_SOURCE 700
#include "scheduler.h"
#include "init.h"
#include "queue.h"
#include "sync.h"
#include "thread.h"
#include <stdlib.h>
#include <time.h>

sem_t *SEM; // 동기화를 위해 세마포어 변수를 전역으로 정의

/* 스레드를 실행 후 중단 시키는 시간 간격의 싱크를 맞추기 위해 약간의
 * 지연을 발생시키는 함수 */
void sync_time_delay(void) {
    for (int i = 0; i < 2000000; i++) {
        int j = i;
    }
}

void __ContextSwitch(int curpid, int newpid) {
    kill(curpid, SIGSTOP);
    sync_time_delay();
    kill(newpid, SIGCONT);
}

/* SIGINT를 수신하면 세마포어를 삭제후 종료 */
void remove_sem_exit(int sig) {
    sem_unlink("mysem");
    sem_close(SEM);
    exit(0);
}

/* SIGABRT를 수신하면 세마포어를 삭제후 종료 */
void remove_sem_abort(int sig) {
    sem_unlink("mysem");
    sem_close(SEM);
    signal(SIGABRT, SIG_DFL);
    abort();
}

void scheduler(int signo, siginfo_t *info, void *context) {
    SEM = sem_open("mysem", O_CREAT, 0644, 1);
    sem_wait(SEM); // 전역 변수에 대한 동기화

    Thread *cur_thread, *new_thread;
    pid_t curpid, newpid;

    if (ReadyQueue.queueCount == 0) {
        if (pCurrentThread != NULL) {
            if (pCurrentThread->status == THREAD_STATUS_ZOMBIE) {
                /* ReadyQueue가 비어있고 스레드가 좀비상태라면 현재 실행중인
                 * 스레드가 없는 것이다 */
                pCurrentThread = NULL;
            } else {
                /* ReadyQueue가 비어있지만 실행중인 스레드가 있다면 계속해서
                 * 실행한다 */
                sync_time_delay();
                pCurrentThread->cpu_time += 2;
            }
        }
    } else if (pCurrentThread != NULL) {
        /* ReadyQueue가 비어있지 않고 현재 실행중인 스레드가 있는 경우 */

        // 만약 새로실행되는 스레드가 thread_sem_wait()이나 thread_sem_post()를
        // 호출하면 ReadyQueue나 pCurrentThread에 접근하기때문에 문제가 발생할수
        // 있다. 따라서 ReadyQueue나 pCurrentThread에 대한 모든 작업을 완료한
        // 뒤에 새로운 스레드에게 SOGCONT를 보내야 한다.
        // -> 즉 __ContextSwitch()를 호출하지 않고 각 스레드에 SIGSTOP과
        // SIGCONT를 따로 보내줘야 한다는 것이다.

        kill(pCurrentThread->pid, SIGSTOP);
        sync_time_delay();

        /* ReadyQueue에서 새로운 스레드를 꺼낸다 */
        new_thread = ReadyQueue.pHead;
        queue_pop(&ReadyQueue);

        /* 새로운 스레드의 TCB 정보 업데이트 */
        new_thread->status = THREAD_STATUS_RUN;
        new_thread->cpu_time += TIMESLICE;

        cur_thread = pCurrentThread; // 실행중이던 스레드를 가리킨다

        /* 새로운 스레드를 현재 실행중인 스레드로 가리키게 한다 */
        pCurrentThread = new_thread;

        /* 현재까지 실행중이었던 스레드가 좀비상태라면 ReadyQueue에 집어넣지
         * 않는다 */
        if (cur_thread->status != THREAD_STATUS_ZOMBIE) {
            queue_push(&ReadyQueue, cur_thread);
            cur_thread->status = THREAD_STATUS_READY;
        }
        kill(new_thread->pid, SIGCONT);

    } else {
        /* ReadyQueue가 비어있지 않지만 현재 실행중인 스레드가 없는 경우 단순히
         * ReadyQueue의 head에 있는 스레드를 실행시키면 된다 */

        /* ReadyQueue에서 꺼내서 현재 실행중인 스레드로 가리키게 한다 */
        pCurrentThread = ReadyQueue.pHead;
        queue_pop(&ReadyQueue);

        /* 새로 실행하는 스레드의 TCB 정보 업데이트 */
        pCurrentThread->cpu_time += TIMESLICE;
        pCurrentThread->status = THREAD_STATUS_RUN;

        sync_time_delay();
        kill(pCurrentThread->pid, SIGCONT);
    }

    sem_post(SEM);
}

void RunScheduler(void) {
    SEM = sem_open("mysem", O_CREAT, 0644, 1);

    struct sigaction act;
    struct itimerspec value;
    timer_t timer_id = 0;

    /* SIGALRM에 대한 시그널 핸들러(scheduler) 등록 */
    act.sa_flags = SA_SIGINFO;
    act.sa_sigaction = scheduler;
    sigemptyset(&act.sa_mask);
    sigaction(SIGALRM, &act, NULL);

    /* 세마포어를 삭제시키고 종료하도록 시그널 핸들러 등록 */
    signal(SIGINT, remove_sem_exit);
    signal(SIGABRT, remove_sem_abort);

    /* 타이머 생성 */
    timer_create(CLOCK_REALTIME, NULL, &timer_id);

    /* 타이머 시작 시간과 타이머 간격 설정 */
    value.it_interval.tv_sec = TIMESLICE;
    value.it_interval.tv_nsec = 0;
    value.it_value.tv_sec = TIMESLICE;
    value.it_value.tv_nsec = 0; // it_value 값을 0으로 설정하면 타이머가 세팅이
                                // 안되기 때문에 2초후 타이머가 시작하도록 설정

    /* 타이머 시작 */
    timer_settime(timer_id, 0, &value, NULL);

    /* 맨 처음에만 타이머 없이 수동으로 스레드를 실행 시킨다 */
    if (ReadyQueue.queueCount != 0) {
        /* ReadyQueue가 비어있지 않은 경우에만 스레드 실행 */
        sem_wait(SEM); // 전역 변수를 위한 동기화

        /* ReadyQueue에서 꺼내서 현재 실행 중인 스레드로 가리키게 한다 */
        pCurrentThread = ReadyQueue.pHead;
        queue_pop(&ReadyQueue);

        /* 새로 실행하는 스레드의 TCB 정보 업데이트 */
        pCurrentThread->cpu_time += TIMESLICE;
        pCurrentThread->status = THREAD_STATUS_RUN;

        sync_time_delay();
        kill(pCurrentThread->pid, SIGCONT);
        sem_post(SEM);
    }
}
