#ifndef TEST_CASE_2_H
#define TEST_CASE_2_H
 
#include <stdio.h>
#include <unistd.h>
#include <assert.h>
#include <time.h>
#include <errno.h>
#include "thread.h"

#define TC2_THREAD_NUM (3)
void* Tc2ThreadProc(void* param);
void TestCase2(void);

#endif

