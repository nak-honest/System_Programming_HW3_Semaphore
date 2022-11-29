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
    SEM = sem_open("mysem", O_CREAT, 0644, 1);
    sem_wait(SEM); // 전역 변수에 대한 동기화

    Thread *new_thread;
    void *stack;
    thread_t tid;
    pid_t new_pid;
    BOOL is_full = 1;

    /* Table의 entry 0부터 빈 entry를 찾는다 */
    for (int i = 0; i < MAX_THREAD_NUM; i++) {
        if (pThreadTbEnt[i].bUsed == 0) {
            is_full = 0;
            tid = i;       // 해당 엔트리의 번호가 thread의 id임.
            *thread = tid; // 생성될 thread id 반환
            break;
        }
    }

    if (is_full == 1) { // Table의 빈 엔트리가 없는 경우
        perror("The number of creatable threads are full!");
        sem_post(SEM);
        return -1;
    }

    stack = malloc(STACK_SIZE);
    new_thread = (Thread *)malloc(sizeof(Thread));

    // 스레드 생성 후 pid 반환
    new_pid = clone(start_routine, (char *)stack + STACK_SIZE,
                    CLONE_VM | CLONE_SIGHAND | CLONE_FS | CLONE_FILES, arg);

    kill(new_pid, SIGSTOP); // 스레드를 생성 후 바로 정지시킨다

    /* TCB 초기화 */
    new_thread->stackSize = STACK_SIZE;
    new_thread->stackAddr = stack;
    new_thread->status = THREAD_STATUS_READY;
    new_thread->pid = new_pid;
    new_thread->cpu_time = 0;

    queue_push(&ReadyQueue, new_thread); // ReadyQueue에 push

    /* Table에 update */
    pThreadTbEnt[tid].pThread = new_thread;
    pThreadTbEnt[tid].bUsed = 1;

    sem_post(SEM);

    return 0;
}

int thread_suspend(thread_t tid) {
    SEM = sem_open("mysem", O_CREAT, 0644, 1);
    sem_wait(SEM); // 전역 변수에 대한 동기화

    // 해당되는 tid의 스레드가 존재하지 않음
    if (pThreadTbEnt[tid].bUsed == 0) {
        perror("thread_suspend() : That tid is not exist!");
        sem_post(SEM);
        return -1;
    }

    // Table의 tid 엔트리에서 스레드의 TCB 포인터를 얻어온다.
    Thread *thread = pThreadTbEnt[tid].pThread;

    // 자기 자신은 suspend 하지 않도록 구현
    if (pCurrentThread == thread) {
        perror("thread_suspend() : self-suspend!");
        sem_post(SEM);
        return -1;
    }

    // 해당 스레드가 Ready 상태인 경우 ReadyQueue에서 WaitingQueue로 이동
    if (thread->status == THREAD_STATUS_READY) {
        queue_remove(&ReadyQueue, thread);
        thread->status = THREAD_STATUS_WAIT;
        queue_push(&WaitingQueue, thread); // Waiting Queue의 tail로 이동
    }

    sem_post(SEM);
    return 0;
}

int thread_cancel(thread_t tid) {
    SEM = sem_open("mysem", O_CREAT, 0644, 1);
    sem_wait(SEM); // 전역 변수에 대한 동기화

    // 해당되는 tid의 스레드가 존재하지 않음
    if (pThreadTbEnt[tid].bUsed == 0) {
        perror("thread_resume() : That tid is not exist!");
        sem_post(SEM);
        return -1;
    }

    // Table의 tid 엔트리에서 스레드의 TCB 포인터를 얻어온다.
    Thread *thread = pThreadTbEnt[tid].pThread;
    pid_t tpid = thread->pid;

    // 자기 자신은 cancle 하지 않도록 구현
    if (pCurrentThread == thread) {
        perror("thread_cancel() : self-cancel!");
        sem_post(SEM);

        return -1;
    }

    kill(tpid, SIGKILL);

    /* Queue에서 제거 */
    if (thread->status == THREAD_STATUS_READY) {
        queue_remove(&ReadyQueue, thread);
    } else if (thread->status == THREAD_STATUS_WAIT) {
        queue_remove(&WaitingQueue, thread);
    }

    /* Table에서 해당 스레드 제거 */
    pThreadTbEnt[tid].bUsed = 0;
    pThreadTbEnt[tid].pThread = NULL;

    /* Stack 및 TCB를 deallocate */
    free(thread->stackAddr);
    thread->stackAddr = NULL;
    free(thread);
    thread = NULL;

    sem_post(SEM);
    return 0;
}

int thread_resume(thread_t tid) {
    SEM = sem_open("mysem", O_CREAT, 0644, 1); // 전역 변수에 대한 동기화
    sem_wait(SEM);

    // 해당되는 tid의 스레드가 존재하지 않음
    if (pThreadTbEnt[tid].bUsed == 0) {
        perror("thread_resume() : That tid is not exist!");
        sem_post(SEM);
        return -1;
    }

    // Table의 tid 엔트리에서 스레드의 TCB 포인터를 얻어온다.
    Thread *thread = pThreadTbEnt[tid].pThread;

    /* WaitingQueue에서 ReadyQueue로 이동 */
    if (thread->status == THREAD_STATUS_WAIT) {
        queue_remove(&WaitingQueue, thread);
        thread->status = THREAD_STATUS_READY;
        queue_push(&ReadyQueue, thread); // ReadyQueue의 tail로 이동
    }

    sem_post(SEM);
    return 0;
}

thread_t thread_self(void) {
    SEM = sem_open("mysem", O_CREAT, 0644, 1); // 전역 변수에 대한 동기화
    sem_wait(SEM);

    thread_t tid;

    /* Table의 모든 엔트리에 대해 TCB 주소 비교 */
    for (int i = 0; i < MAX_THREAD_NUM; i++) {
        if (pThreadTbEnt[i].pThread == pCurrentThread) {
            tid = i; // 찾으면 tid 할당
        }
    }

    sem_post(SEM);
    return tid;
}

void disjoin(int signo) {}

int thread_join(thread_t tid) {
    SEM = sem_open("mysem", O_CREAT, 0644, 1); // 전역 변수에 대한 동기화
    sem_wait(SEM);

    Thread *new_thread, *parent_thread, *child_thread;
    pid_t curpid, newpid;

    parent_thread = pCurrentThread; // 현재 이 함수를 호출한 스레드가 부모임
    child_thread = pThreadTbEnt[tid].pThread; // tid에 해당하는 스레드가 자식임

    signal(SIGCHLD, disjoin); // SIGCHLD에 대한 시그널 등록

    /* parent를 WaitingQueue로 이동시킨다. */
    pCurrentThread->status = THREAD_STATUS_WAIT;
    queue_push(&WaitingQueue, parent_thread);

    // ReadyQueue가 비어있지 않은 경우에만 새로운 스레드를 실행시킬수 있다
    if (ReadyQueue.queueCount != 0) {
        new_thread = ReadyQueue.pHead;
        queue_pop(&ReadyQueue); // ReadyQueue의 head에서 스레드를 가져온다

        /* 새로운 스레드 TCB 업데이트 */
        new_thread->status = THREAD_STATUS_RUN;
        new_thread->cpu_time += 2;

        newpid = new_thread->pid;
        kill(newpid, SIGCONT);       // 새로운 스레드 실행
        pCurrentThread = new_thread; // pCurrentThread 업데이트
    } else {
        pCurrentThread = NULL; // ReadyQueue가 비어있는 경우 아무것도 실행안함
    }

    sem_post(SEM); // parent가 pause 상태에 들어가기 때문에 동기화 해제

    while (child_thread->status != THREAD_STATUS_ZOMBIE) {
        pause(); // SIGCHLD를 받으면 깨어나서 자식이 종료된건지 확인
    }

    sem_wait(SEM); // parent가 깨어나면 다시 동기화를 건다

    /* parent를 WaitingQueue에서 ReadyQueue로 보낸다 */
    queue_remove(&WaitingQueue, parent_thread);
    queue_push(&ReadyQueue, parent_thread);
    parent_thread->status = THREAD_STATUS_READY;

    // pCurrentThread = NULL; // 자식이 죽었으므로 현재 실행중인 스레드가 없도록

    /* 자식 스레드의 Stack과 TCB를 deallocate */
    free(child_thread->stackAddr);
    child_thread->stackAddr = NULL;
    free(child_thread);
    child_thread = NULL;

    // Table에서 child thread 삭제
    pThreadTbEnt[tid].bUsed = 0;
    pThreadTbEnt[tid].pThread = NULL;

    sem_post(SEM);
    kill(parent_thread->pid, SIGSTOP); // 위의 작업을 모두 수행 후 자신을 정지

    return 0;
}

int thread_cputime(void) {
    SEM = sem_open("mysem", O_CREAT, 0644, 1); // 전역 변수에 대한 동기화
    sem_wait(SEM);

    int time = (int)pCurrentThread->cpu_time;

    sem_post(SEM);
    return time;
}

void thread_exit(void) { pCurrentThread->status = THREAD_STATUS_ZOMBIE; }
