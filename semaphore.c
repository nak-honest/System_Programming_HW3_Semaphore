#include "semaphore.h"
#include "queue.h"
#include "thread.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

SemaphoreTblEnt pSemaphoreTblEnt[MAX_SEMAPHORE_NUM];

int thread_sem_open(char *name, int count) {
    int sem_id = -1;
    BOOL is_full = 1;
    BOOL is_find = 0;
    Semaphore *new_sem;

    for (int i = 0; i < MAX_SEMAPHORE_NUM; i++) {
        if (strcmp(name, pSemaphoreTblEnt[i].name) == 0) {
            is_find = 1;
            sem_id = i; // 해당 이름의 세마포어 엔트리 번호가 id임
            break;
        }
    }

    if (is_find) { // 이미 존재하는 세마포어라면 id만 반환
        return sem_id;
    }

    // 위에서 return을 하지 않았다면 세마포어를 새로 생성해야 함
    for (int i = 0; i < MAX_SEMAPHORE_NUM; i++) {
        if (pSemaphoreTblEnt[i].bUsed == 0) {
            is_full = 0;
            sem_id = i; // 해당 엔트리의 번호가 세마포어의 id임
            break;
        }
    }

    if (is_full) { // Table의 빈 엔트리가 없는 경우
        perror("The number of creatable semaphores are full!");
        return -1;
    }

    new_sem = (Semaphore *)malloc(sizeof(Semaphore));

    /* 해당 세마포어의 SCB 초기화 */
    new_sem->count = count;
    new_sem->waitingQueue.queueCount = 0;
    new_sem->waitingQueue.pHead = NULL;
    new_sem->waitingQueue.pTail = NULL;

    /* Table에 update */
    strcpy(pSemaphoreTblEnt[sem_id].name, name);
    pSemaphoreTblEnt[sem_id].bUsed = 1;
    pSemaphoreTblEnt[sem_id].pSemaphore = new_sem;

    return sem_id;
}

int thread_sem_wait(int semid) {
    Thread *cur_thread = pCurrentThread;
    Thread *new_thread = NULL;
    Semaphore *sem;
    pid_t cur_pid = cur_thread->pid;
    pid_t new_pid;

    sem = pSemaphoreTblEnt[semid].pSemaphore;

    if (sem->count > 0) {
        // sem->count만 1 감소시키면 된다.
        sem->count--;
        return 0;
    }

    // 다른 스레드가 thread_sem_post()를 호출하면 Waiting Queue의 head에 있는
    // 스레드가 ReadyQueue의 tail로 들어가야 한다. 그런데 ReadyQueue에 있는
    // 사이에 sem->count가 다시 0이 될수 있기 때문에 실행될때마다 while문을 통해
    // sem->count가 0인지를 체크해야한다.
    while (sem->count == 0) {
        // sem->count가 0이면 현재 스레드는 wait 상태에 빠져야 한다. 그러면
        // 그동안 CPU가 아무일도 하지 않기때문에 ReadyQueue에 있는 스레드를 새로
        // 생성해야한다.
        if (ReadyQueue.queueCount != 0) {
            /* ReadyQueue가 비어있지 않은 경우에만 새로운 스레드를 실행할 수
             * 있다 */
            new_thread = ReadyQueue.pHead;
            queue_pop(&ReadyQueue); // ReadyQueue에서 새로운 스레드를 꺼낸다

            // pCurrentThread가 새로운 스레드를 가리키게 한다
            pCurrentThread = new_thread;

            // 새로운 스레드의 상태 업데이트
            new_thread->status = THREAD_STATUS_RUN;
            new_thread->cpu_time += 2;

            new_pid = new_thread->pid;

            // 현재 스레드를 wait 상태로 만든다
            cur_thread->status = THREAD_STATUS_WAIT;
            queue_push(&sem->waitingQueue, cur_thread);
        } else {
            /* ReadyQueue가 비어있는 경우 아무것도 실행하지 않는다 */
            pCurrentThread = NULL;

            // 현재 스레드를 wait 상태로 만든다
            cur_thread->status = THREAD_STATUS_WAIT;
            queue_push(&sem->waitingQueue, cur_thread);
        }

        if (new_thread != NULL) {
            kill(new_pid, SIGCONT); // 새로운 스레드를 실행시킨다
            kill(cur_pid, SIGSTOP); // 현재 스레드를 정지시킨다
        }
    }

    // sem->count가 0보다 커서 위의 while문을 빠져나오면, 해당 스레드가
    // thread_sem_wait() 호출에 성공하는 것이므로 sem->count를 1 감소시켜야
    // 한다.
    sem->count--;

    return 0;
}

int thread_sem_post(int semid) {
    Thread *wait_thread;
    Semaphore *sem;

    sem = pSemaphoreTblEnt[semid].pSemaphore;
    sem->count++; // sem->count를 1 증가시킨다.

    if (sem->waitingQueue.queueCount != 0) {
        // thread_sem_post()를 호출할때마다 WaitingQueue에서 스레드를 하나 꺼내
        // ReadyQueue에 넣는다
        wait_thread = sem->waitingQueue.pHead;
        queue_pop(&sem->waitingQueue);
        queue_push(&ReadyQueue, wait_thread);
        wait_thread->status = THREAD_STATUS_READY;
    }

    return 0;
}

int thread_sem_close(int semid) {
    if (pSemaphoreTblEnt[semid].bUsed) {
        memset(pSemaphoreTblEnt[semid].name, '\0', SEM_NAME_MAX);
        pSemaphoreTblEnt[semid].bUsed = 0;
        free(pSemaphoreTblEnt[semid].pSemaphore);
        pSemaphoreTblEnt[semid].pSemaphore = NULL;
    }

    return 0;
}
