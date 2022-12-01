#ifndef __SYNC_H__
#define __SYNC_H__

#include "thread.h"
#include <fcntl.h>
#include <semaphore.h>

extern sem_t *SEM;

void sync_time_delay(void);

#endif // __SYNC_H__
