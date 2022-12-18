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
        sem->count--;
        return 0;
    }

    while (sem->count == 0) {
        if (ReadyQueue.queueCount != 0) {
            /* ReadyQueue가 비어있지 않은 경우에만 새로운 스레드를 실행할 수
             * 있다 */
            new_thread = ReadyQueue.pHead;
            queue_pop(&ReadyQueue); // ReadyQueue에서 새로운 스레드를 꺼낸다
            pCurrentThread = new_thread;

            new_thread->status = THREAD_STATUS_RUN;
            new_thread->cpu_time += 2; // 새로운 스레드의 상태 업데이트

            new_pid = new_thread->pid;

            // pCurrentThread가 새로운 스레드를 가리키게 한다
            cur_thread->status = THREAD_STATUS_WAIT;
            queue_push(&sem->waitingQueue, cur_thread);
        } else {
            /* ReadyQueue가 비어있는 경우 아무것도 실행하지 않는다 */
            pCurrentThread = NULL;
            cur_thread->status = THREAD_STATUS_WAIT;
            queue_push(&sem->waitingQueue, cur_thread);
        }

        if (new_thread != NULL) {
            kill(new_pid, SIGCONT); // 새로운 스레드를 실행시킨다
            kill(cur_pid, SIGSTOP);
        }
    }

    sem->count--;

    return 0;
}

int thread_sem_post(int semid) {
    Thread *wait_thread;
    Semaphore *sem;

    sem = pSemaphoreTblEnt[semid].pSemaphore;
    sem->count++;

    if (sem->waitingQueue.queueCount != 0) {
        wait_thread = sem->waitingQueue.pHead;
        queue_pop(&sem->waitingQueue);
        queue_push(&ReadyQueue, wait_thread);
        wait_thread->status = THREAD_STATUS_READY;
    }
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
