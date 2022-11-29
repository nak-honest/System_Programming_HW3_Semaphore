#include "init.h"
#include "queue.h"
#include "scheduler.h"
#include "thread.h"
#include <linux/sched.h>
#include <sched.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>

void *foo(void *arg) {
    while (1) {
        printf("%d\n", thread_self());
    }
    return NULL;
}

void main() {
    Init();
    thread_t tid = -1;
    thread_t tid2 = -1;
    thread_t tid3 = -1;

    int a = 1;
    int b = 2;
    int c = 3;

    thread_create(&tid, NULL, foo, &a);
    thread_create(&tid2, NULL, foo, &b);
    thread_create(&tid3, NULL, foo, &c);

    RunScheduler();
    while (1) {
    }
}
