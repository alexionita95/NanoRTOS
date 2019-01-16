/*
 * mutex.c
 *
 * Created: 1/8/2019 8:18:07 PM
 *  Author: Alex Ionita
 */ 
#include <avr/interrupt.h>

#include "mutex.h"

#include "task.h"

void mutexInit(Mutex *m) {
	m->status = MUTEX_UNLOCKED;
	QUEUE_INIT(&m->waiting);
}

void mutexLock(Mutex *m) {
	uint8_t sreg;

	sreg = SREG;
	cli();

	if (m->status == MUTEX_LOCKED) {
		taskSuspend(&m->waiting);
		} else {
		m->status = MUTEX_LOCKED;
	}

	SREG = sreg;
}

void mutexUnlock(Mutex *m) {
	uint8_t sreg;
	QUEUE *q;
	Task *t;

	sreg = SREG;
	cli();

	if (QUEUE_EMPTY(&m->waiting)) {
		m->status = MUTEX_UNLOCKED;
		} else {
		q = QUEUE_HEAD(&m->waiting);
		t = QUEUE_DATA(q, Task, member);
		QUEUE_REMOVE(q);
		taskWakeup(t);
	}

	SREG = sreg;
}
