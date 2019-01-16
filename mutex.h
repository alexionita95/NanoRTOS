/*
 * mutex.h
 *
 * Created: 1/8/2019 8:18:24 PM
 *  Author: Alex Ionita
 */ 


#ifndef MUTEX_H_
#define MUTEX_H_
#define MUTEX_UNLOCKED 0
#define MUTEX_LOCKED 1
#include "queue.h"

typedef struct MutexStruct Mutex;

struct MutexStruct {
	unsigned status:1;
	QUEUE waiting;
};

void mutexInit(Mutex *mutex);

void mutexLock(Mutex *mutex);

void mutexUnlock(Mutex *mutex);





#endif /* MUTEX_H_ */