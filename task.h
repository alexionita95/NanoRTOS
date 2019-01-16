/*
 * task.h
 *
 * Created: 1/8/2019 7:37:40 PM
 *  Author: Alex Ionita
 */ 


#ifndef TASK_H_
#define TASK_H_

#include <stdint.h>

#include "queue.h"
#define F_CPU 16000000L
#ifndef F_CPU
#error "Define F_CPU"
#endif

#define MS_PER_TICK 2
#define US_PER_TICK (1000 * MS_PER_TICK)
#define US_PER_COUNT (US_PER_TICK / COUNTS_PER_TICK)

#if F_CPU == 16000000L
// Clock select: prescaler = 1/256
#define _TCCR0B (_BV(CS02))
#define COUNTS_PER_TICK ((F_CPU / 256) / (1000 / MS_PER_TICK))
#elif F_CPU == 8000000L
// Clock select: prescaler = 1/64
#define _TCCR0B (_BV(CS01) | _BV(CS00))
#define COUNTS_PER_TICK ((F_CPU / 64) / (1000 / MS_PER_TICK))
#else
#error "Unsupported F_CPU"
#endif

typedef void (*TaskFunction)(void *);

typedef struct TaskStruct Task;

struct TaskStruct {
	void *stackPointer; // Stack pointer this task can be resumed from.
	uint16_t delay; // Ticks until task can be scheduled again.

	QUEUE member;
};

void taskInit(void);

Task *taskCreate(TaskFunction fn, void *data);

void taskStart(void);


void taskYield(void);


Task *taskCurrent(void);

void taskSuspend(QUEUE *h);


void taskWakeup(Task *t);

void taskSleep(uint16_t ms);

#if TASK_COUNT_SEC
#ifndef TASK_SEC_T
#define TASK_SEC_T uint16_t
#endif
TASK_SEC_T taskAddSecond(void);
void taskSetSecond(TASK_SEC_T);
#endif

#if TASK_COUNT_MSEC
#ifndef TASK_MSEC_T
#define TASK_MSEC_T uint16_t
#endif
TASK_MSEC_T taskAddMilisecond(void);
void taskSetMilisecond(TASK_MSEC_T);
#endif


#if TASK_COUNT_USEC
#ifndef TASK_USEC_T
#define TASK_USEC_T uint16_t
#endif
TASK_USEC_T taskAddMicrosecond(void);
void taskSetMicrosecond(TASK_USEC_T);
#endif




#endif /* TASK_H_ */