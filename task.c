/*
 * task.c
 *
 * Created: 1/8/2019 7:38:36 PM
 *  Author: Alex Ionita
 */ 
#include <avr/interrupt.h>
#include <avr/io.h>

#include "task.h"


static Task *currentTask = 0;


static QUEUE readyTasks;


static QUEUE suspendedTasks;


static QUEUE sleepingTasks;

#if TASK_COUNT_SEC
static TASK_SEC_T _task_sec = 0;

// Counts down until a second has passed.
static uint16_t _task_sec_countdown;

TASK_SEC_T taskAddSecond(void) {
	return _task_sec;
}

void taskSetSecond(TASK_SEC_T t) {
	uint8_t sreg = SREG;

	cli();
	_task_sec = t;
	_task_sec_countdown = 1000 / MS_PER_TICK;
	SREG = sreg;
}
#endif // TASK_COUNT_SEC

#if TASK_COUNT_MSEC
static TASK_MSEC_T _task_msec = 0;

TASK_MSEC_T taskAddMilisecond(void) {
	return _task_msec;
}

void taskSetMilisecond(TASK_MSEC_T t) {
	uint8_t sreg = SREG;

	cli();
	_task_msec = t;
	SREG = sreg;
}
#endif // TASK_COUNT_MSEC

#if TASK_COUNT_USEC
static TASK_USEC_T _task_usec = 0;

TASK_USEC_T taskAddMicrosecond(void) {
	return (_task_usec + (TCNT0 * US_PER_COUNT));
}

void taskSetMicrosecond(TASK_USEC_T t) {
	uint8_t sreg = SREG;

	cli();
	_task_usec = t;
	SREG = sreg;
}
#endif // TASK_COUNT_USEC

// Push a task's context onto its own stack.
static inline void taskPush(void) __attribute__ ((always_inline));
static inline void taskPush(void) {
	asm volatile(
	"push r0\n"
	// Save status register
	"in r0, 0x3f\n"
	"cli\n"
	"push r0\n"
	// Save Z register pair (so it can be used below)
	"push r30\n"
	"push r31\n"
	// Load currentTask into Z register pair
	"lds r30, currentTask\n" // Low
	"lds r31, currentTask+1\n" // High
	// Z=1 if (r30 | r31) == 0
	"mov r0, r30\n"
	"or r0, r31\n"
	// Save if Z=0 (currentTask != NULL)
	"brne 2f\n"
	"1:\n"
	// Restore Z register pair
	"pop r31\n"
	"pop r30\n"
	// Restore status register and real r0
	"pop r0\n"
	"out 0x3f, r0\n"
	"pop r0\n"
	"rjmp 3f\n"
	"2:\n"
	// Save general registers
	"push r1\n"
	"push r2\n"
	"push r3\n"
	"push r4\n"
	"push r5\n"
	"push r6\n"
	"push r7\n"
	"push r8\n"
	"push r9\n"
	"push r10\n"
	"push r11\n"
	"push r12\n"
	"push r13\n"
	"push r14\n"
	"push r15\n"
	"push r16\n"
	"push r17\n"
	"push r18\n"
	"push r19\n"
	"push r20\n"
	"push r21\n"
	"push r22\n"
	"push r23\n"
	"push r24\n"
	"push r25\n"
	"push r26\n"
	"push r27\n"
	"push r28\n"
	"push r29\n"
	// Compiler expects r1 to be zero. As this code may interrupt anything,
	// the register may temporarily hold a non-zero value.
	"clr r1\n"
	// Save stack pointer in current task struct
	"in r0, 0x3d\n" // Low
	"st z+, r0\n"
	"in r0, 0x3e\n" // High
	"st z+, r0\n"
	"3:\n"
	);
}

// Pop a task's context off of its own stack and resume it.
static void taskPop(void) __attribute__ ((naked));
static void taskPop(void) {
	asm volatile(
	// Restore stack pointer from current task struct
	"lds r26, currentTask\n" // Low
	"lds r27, currentTask+1\n" // High
	"ld r0, x+\n"
	"out 0x3d, r0\n" // Low
	"ld r0, x+\n"
	"out 0x3e, r0\n" // High
	// Restore general registers
	"pop r29\n"
	"pop r28\n"
	"pop r27\n"
	"pop r26\n"
	"pop r25\n"
	"pop r24\n"
	"pop r23\n"
	"pop r22\n"
	"pop r21\n"
	"pop r20\n"
	"pop r19\n"
	"pop r18\n"
	"pop r17\n"
	"pop r16\n"
	"pop r15\n"
	"pop r14\n"
	"pop r13\n"
	"pop r12\n"
	"pop r11\n"
	"pop r10\n"
	"pop r9\n"
	"pop r8\n"
	"pop r7\n"
	"pop r6\n"
	"pop r5\n"
	"pop r4\n"
	"pop r3\n"
	"pop r2\n"
	"pop r1\n"
	// Restore Z register pair
	"pop r31\n"
	"pop r30\n"

	// Restore status register.

	"pop r0\n"
	"sbrs r0, 7\n" // Skip if bit in register set
	"jmp task_pop_ret\n"
	"jmp task_pop_reti\n"
	"task_pop_ret:\n"
	"out 0x3f, r0\n" // Restore status register
	"pop r0\n" // Restore the real r0
	"ret\n"
	"task_pop_reti:\n"
	"clt\n" // Clear T in SREG
	"bld r0, 7\n" // Bit load from T to r0 bit 7 (interrupt bit)
	"out 0x3f, r0\n" // Restore status register (without interrupt bit set)
	"pop r0\n" // Restore the real r0
	"reti\n"
	);
}


static void *taskInitializeInternal(void *sp, TaskFunction fn, void *data) {
	void *result;

	asm volatile(
	// About to overwrite the stack pointer, disable interrupts
	"in r18, 0x3f\n" // r18 can be clobbered
	"cli\n"
	// Save current stack pointer
	"in r26, 0x3d\n"
	"in r27, 0x3e\n"
	// Set new task's stack pointer
	"out 0x3d, %A1\n"
	"out 0x3e, %B1\n"
	// Store location of task body as return address, such that
	// executing "ret" after "context_restore" will jump to it.
	"push %A2\n"
	"push %B2\n"
	#ifdef __AVR_3_BYTE_PC__
	// Push extra zero, assuming the task function is not located
	// in high program memory (>128KiB).
	"clr __tmp_reg__\n"
	"push __tmp_reg__\n"
	#endif
	// Store r0
	"ldi r19, 0\n" // r19 can be clobbered
	"push r19\n"
	// Store status register
	"ldi r19, 0x80\n" // Start task with interrupts enabled
	"push r19\n"
	// Store general registers
	"ldi r19, 0\n"
	"push r19\n" // r30
	"push r19\n" // r31
	"push r19\n" // r1
	"push r19\n" // r2
	"push r19\n" // r3
	"push r19\n" // r4
	"push r19\n" // r5
	"push r19\n" // r6
	"push r19\n" // r7
	"push r19\n" // r8
	"push r19\n" // r9
	"push r19\n" // r10
	"push r19\n" // r11
	"push r19\n" // r12
	"push r19\n" // r13
	"push r19\n" // r14
	"push r19\n" // r15
	"push r19\n" // r16
	"push r19\n" // r17
	"push r19\n" // r18
	"push r19\n" // r19
	"push r19\n" // r20
	"push r19\n" // r21
	"push r19\n" // r22
	"push r19\n" // r23
	// Argument to task function
	"push %A3\n" // r24
	"push %B3\n" // r25
	"push r19\n" // r26
	"push r19\n" // r27
	"push r19\n" // r28
	"push r19\n" // r29
	// Store new task's stack pointer at return register
	"in %A0, 0x3d\n"
	"in %B0, 0x3e\n"
	// Restore stack pointer
	"out 0x3d, r26\n"
	"out 0x3e, r27\n"
	// Restore status register
	"out 0x3f, r18\n"
	: "=r" (result)
	: "r" (sp), "r" (fn), "r" (data)
	: "r18", "r19", "r26", "r27"
	);

	return result;
}


Task *taskCreateInternal(TaskFunction fn, void *data) {
	static void *start = (void *)RAMEND;
	void *sp;
	Task *t;


	start -= 0x100;


	t = start - sizeof(Task);
	sp = (void *)t - 1;

	t->stackPointer = taskInitializeInternal(sp, fn, data);
	t->delay = 0;
	QUEUE_INIT(&t->member);

	return t;
}


Task *taskCreate(TaskFunction fn, void *data) {
	Task *t = taskCreateInternal(fn, data);

	QUEUE_INSERT_TAIL(&readyTasks, &t->member);

	return t;
}

static void taskTick() {
	QUEUE *q, *r;
	Task *t;

	#if TASK_COUNT_SEC
	if (--_task_sec_countdown == 0) {
		_task_sec++;
		_task_sec_countdown = 1000 / MS_PER_TICK;
	}
	#endif

	#if TASK_COUNT_MSEC
	_task_msec += MS_PER_TICK;
	#endif

	#if TASK_COUNT_USEC
	_task_usec += US_PER_TICK;
	#endif

	q = QUEUE_NEXT(&sleepingTasks);
	r = 0;
	for (; q != &sleepingTasks; q = r) {

		r = QUEUE_NEXT(q);
		t = QUEUE_DATA(q, Task, member);

		if (t->delay) {
			t->delay--;
		}

		if (t->delay == 0) {
			taskWakeup(t);
		}
	}
}

static void taskScheduler(void) {

	asm volatile(
	"out 0x3d, %A0\n"
	"out 0x3e, %B0\n"
	:: "x" (RAMEND)
	);

	for (;;) {
		QUEUE *q;

		currentTask = 0;


		QUEUE_FOREACH(q, &readyTasks) {

			currentTask = QUEUE_DATA(q, Task, member);

			QUEUE_ROTATE(&readyTasks, q);


			taskPop();
		}


		sei();
		asm volatile ("sleep");
		cli();
	}
}

static void taskJmpScheduler(void) {
	asm volatile ("ijmp" :: "z" (taskScheduler));
}


static void taskYieldFromTimer(void) __attribute__((naked));
static void taskYieldFromTimer(void) {
	taskPush();

	taskTick();

	taskJmpScheduler();
}

ISR(TIMER0_COMPA_vect, ISR_NAKED) {
	taskYieldFromTimer();

	asm volatile ("reti");
}

static void task__setup_timer() {
	// Waveform generation mode: CTC
	// WGM02: 0
	// WGM01: 1
	// WGM00: 0
	TCCR0A = _BV(WGM01);

	TCCR0B = _TCCR0B;

	OCR0A = COUNTS_PER_TICK - 1;
}

void taskInit(void) {
	QUEUE_INIT(&readyTasks);
	QUEUE_INIT(&suspendedTasks);
	QUEUE_INIT(&sleepingTasks);

	task__setup_timer();

	#if TASK_COUNT_SEC
	taskSetSecond(0);
	#endif

	#if TASK_COUNT_MSEC
	taskSetMilisecond(0);
	#endif

	#if TASK_COUNT_USEC
	taskSetMicrosecond(0);
	#endif
}


void taskStart(void) {

	cli();


	TIMSK0 |= _BV(OCIE0A);


	taskJmpScheduler();
}

void taskYield(void) __attribute__((naked));
void taskYield(void) {
	taskPush();

	taskJmpScheduler();
}


Task *taskCurrent(void) {
	return currentTask;
}

void taskSuspendInternal(QUEUE *h) {
	uint8_t sreg = SREG;

	cli();

	QUEUE *q = &currentTask->member;
	QUEUE_REMOVE(q);
	QUEUE_INSERT_TAIL(h, q);

	taskYield();

	SREG = sreg;
}


void taskSuspend(QUEUE *h) {
	if (h == 0) {
		h = &suspendedTasks;
	}

	taskSuspendInternal(h);
}

// Wake up task.
void taskWakeup(Task *t) {
	uint8_t sreg = SREG;

	cli();

	QUEUE *q = &t->member;
	QUEUE_REMOVE(q);
	QUEUE_INSERT_TAIL(&readyTasks, q);

	SREG = sreg;
}

// Make current task sleep for specified number of ticks.
void taskSleep(uint16_t ms) {
	currentTask->delay = ms / MS_PER_TICK;
	taskSuspendInternal(&sleepingTasks);
}
