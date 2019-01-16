/*
 * os.h
 *
 * Created: 1/8/2019 7:06:59 PM
 *  Author: Alex Ionita
 */ 


#ifndef OS_H_
#define OS_H_


#include <stdint.h>

#include "config.h"

typedef enum { TASK_READY, TASK_SEMAPHORE, TASK_QUEUE } TaskStatus;

typedef struct Task {
    void *sp;
    TaskStatus status;
    struct Task *next;
    void *status_pointer;
} Task;

typedef void (*TaskFn)(void);

extern Task *current_task;

/**
 * Initializes the KOS kernel
 */
void init(void);

/**
 * Creates a new task
 * Note: Not safe
 */
void new_task(TaskFn task, void *sp);

/**
 * Puts KOS in ISR mode
 * Note: Not safe, assumes non-nested isrs
 */
void isr_enter(void);

/**
 * Leaves ISR mode, possibly executing the dispatcher
 * Note: Not safe, assumes non-nested isrs
 */
void isr_exit(void);

#ifdef SEMAPHORE

typedef struct {
    int8_t value;
} Semaphore;

/**
 * Initializes a new semaphore
 */
Semaphore *semaphore_init(int8_t value);

/**
 * Posts to a semaphore
 */
void semaphore_post(Semaphore *sem);

/**
 * Pends from a semaphore
 */
void semaphore_pend(Semaphore *sem);

#endif //SEMAPHORE

#ifdef QUEUE

typedef struct {
    void **messages;
    uint8_t pendIndex;
    uint8_t postIndex;
    uint8_t size;
} Queue;

/**
 * Initializes a new queue
 */
Queue *queue_init(void **messages, uint8_t size);

/**
 * Posts to a queue
 */
void queue_post(Queue *queue, void *message);

/**
 * Pends from a queue
 */
void *queue_pend(Queue *queue);

#endif //QUEUE

/**
 * Runs the kernel
 */
void run(void);

/**
 * Runs the scheduler
 */
void schedule(void);

/**
 * Dispatches the passed task, saving the context of the current task
 */
void dispatch(Task *next);



#endif /* OS_H_ */