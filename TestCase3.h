#ifndef TEST_CASE_3_H
#define TEST_CASE_3_H

#include <stdio.h>
#include <unistd.h>
#include <assert.h>
#include <time.h>
#include <errno.h>
#include "thread.h"

#define TC3_THREAD_NUM (6)
void* Tc3ThreadProc(void* param);
void TestCase3(void);

#endif
