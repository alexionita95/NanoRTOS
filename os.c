/*
 * os.c
 *
 * Created: 1/8/2019 7:09:09 PM
 *  Author: Alex Ionita
 */ 
/**
 * Main code for KOS
 */

#include <avr/interrupt.h>
#include <util/atomic.h>

#include "os.h"
#include "config.h"

/**
 * Context frame format:
 *
 * PC_LOW
 * PC_HIGH
 * r0-31
 * SREG
 */

static Task tasks[MAX_TASKS + 1];
static uint8_t next_task = 0;
static Task *task_head;
Task *current_task;

static uint8_t idle_task_stack[IDLE_TASK_STACK];
static void idle_task(void)
{
    while (1) { }
}

void init(void)
{
    new_task(&idle_task, &idle_task_stack[IDLE_TASK_STACK - 1]);
}

void new_task(TaskFn task, void *sp)
{
    int8_t i;
    uint8_t *stack = sp;
    Task *tcb;

    //make space for pc, sreg, and 32 register
    stack[0] = (uint16_t)task & 0xFF;
    stack[-1] = (uint16_t)task >> 8;
    for (i = -2; i > -34; i--)
    {
        stack[i] = 0;
    }
    stack[-34] = 0x80; //sreg, interrupts enabled
    
    //create the task structure
    tcb = &tasks[next_task++];
    tcb->sp = stack - 35;
    tcb->status = TASK_READY;

    //insert into the task list as the new highest priority task
    if (task_head)
    {
        tcb->next = task_head;
        task_head = tcb;
    }
    else
    {
        task_head = tcb;
    }
}

static uint8_t isr_level = 0;
void isr_enter(void)
{
    isr_level++;
}

void isr_exit(void)
{
    isr_level--;
    schedule();
}

#ifdef SEMAPHORE

static Semaphore semaphores[MAX_SEMAPHORES + 1];
static uint8_t next_semaphore = 0;

Semaphore *semaphore_init(int8_t value)
{
    Semaphore *s = &semaphores[next_semaphore++];
    s->value = value;
    return s;
}

void semaphore_post(Semaphore *semaphore)
{
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
    {
        Task *task;
        semaphore->value++;

        //allow one task to be resumed which is waiting on this semaphore
        task = task_head;
        while (task)
        {
            if (task->status == TASK_SEMAPHORE && task->status_pointer == semaphore)
                break; //this is the task to be restored
            task = task->next;
        }

        task->status = TASK_READY;
        schedule();
    }
}

void semaphore_pend(Semaphore *semaphore)
{
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
    {
        int8_t val = semaphore->value--; //val is value before decrement

        if (val <= 0)
        {
            //we need to wait on the semaphore
            current_task->status_pointer = semaphore;
            current_task->status = TASK_SEMAPHORE;

            schedule();
        }
    }
}

#endif //SEMAPHORE

#ifdef QUEUE

#define NEXT_INDEX(I,S) ((I) < ((S) - 1) ? (I) + 1 : 0)

static Queue queues[MAX_QUEUES + 1];
static uint8_t next_queue = 0;

Queue *queue_init(void **messages, uint8_t size)
{
    Queue *queue = &queues[next_queue++];

    queue->messages = messages;
    queue->pendIndex = queue->postIndex = 0;
    queue->size = size;

    return queue;
}

void queue_post(Queue *queue, void *message)
{
    Task *task;

    ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
    {
        queue->messages[queue->postIndex] = message;
        queue->postIndex = NEXT_INDEX(queue->postIndex, queue->size);

        task = task_head;
        while (task)
        {
            if (task->status == TASK_QUEUE && task->status_pointer == queue)
                break; //this is the task to be restored
        }
        task->status = TASK_READY;
        schedule();
    }
}

void *queue_pend(Queue *queue)
{
    void *data;
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
    {
        if (queue->pendIndex == queue->postIndex)
        {
            //queue is empty, wait for next item
            current_task->status_pointer = queue;
            current_task->status = TASK_QUEUE;
            schedule();
        }
        data = queue->messages[queue->pendIndex];
        queue->pendIndex = NEXT_INDEX(queue->pendIndex, queue->size);
    }
    
    return data;
}

#endif //QUEUE

void run(void)
{
    schedule();
}

void schedule(void)
{
    if (isr_level)
        return;

    Task *task = task_head;
    while (task->status != TASK_READY)
        task = task->next;

    if (task != current_task)
    {
        ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
        {
            dispatch(task);
        }
    }
}

void dispatch(Task *task)
{
    // the call to this function should push the return address into the stack.
    // we will now construct saving context. The entire context needs to be
    // saved because it is very possible that this could be called from within
    // an isr that doesn't use the call-used registers and therefore doesn't
    // save them.
    asm volatile (
            "push r31 \n\t"
            "push r30 \n\t"
            "push r29 \n\t"
            "push r28 \n\t"
            "push r27 \n\t"
            "push r26 \n\t"
            "push r25 \n\t"
            "push r24 \n\t"
            "push r23 \n\t"
            "push r22 \n\t"
            "push r21 \n\t"
            "push r20 \n\t"
            "push r19 \n\t"
            "push r18 \n\t"
            "push r17 \n\t"
            "push r16 \n\t"
            "push r15 \n\t"
            "push r14 \n\t"
            "push r13 \n\t"
            "push r12 \n\t"
            "push r11 \n\t"
            "push r10 \n\t"
            "push r9 \n\t"
            "push r8 \n\t"
            "push r7 \n\t"
            "push r6 \n\t"
            "push r5 \n\t"
            "push r4 \n\t"
            "push r3 \n\t"
            "push r2 \n\t"
            "push r1 \n\t"
            "push r0 \n\t"
            "in   r0, %[_SREG_] \n\t" //push sreg
            "push r0 \n\t"
            "lds  r26, current_task \n\t"
            "lds  r27, current_task+1 \n\t"
            "sbiw r26, 0 \n\t"
            "breq 1f \n\t" //null check, skip next section
            "in   r0, %[_SPL_] \n\t"
            "st   X+, r0 \n\t"
            "in   r0, %[_SPH_] \n\t"
            "st   X+, r0 \n\t"
            "1:" //begin dispatching
            "mov  r26, %A[_next_task_] \n\t"
            "mov  r27, %B[_next_task_] \n\t"
            "sts  current_task, r26 \n\t" //set current task
            "sts  current_task+1, r27 \n\t"
            "ld   r0, X+ \n\t" //load stack pointer
            "out  %[_SPL_], r0 \n\t"
            "ld   r0, X+ \n\t"
            "out  %[_SPH_], r0 \n\t"
            "pop  r31 \n\t" //status into r31: andi requires register above 15
            "bst  r31, %[_I_] \n\t" //we don't want to enable interrupts just yet, so store the interrupt status in T
            "bld  r31, %[_T_] \n\t" //T flag is on the call clobber list and tasks are only blocked as a result of a function call
            "andi r31, %[_nI_MASK_] \n\t" //I is now stored in T, so clear I
            "out  %[_SREG_], r31 \n\t"
            "pop  r0 \n\t"
            "pop  r1 \n\t"
            "pop  r2 \n\t"
            "pop  r3 \n\t"
            "pop  r4 \n\t"
            "pop  r5 \n\t"
            "pop  r6 \n\t"
            "pop  r7 \n\t"
            "pop  r8 \n\t"
            "pop  r9 \n\t"
            "pop  r10 \n\t"
            "pop  r11 \n\t"
            "pop  r12 \n\t"
            "pop  r13 \n\t"
            "pop  r14 \n\t"
            "pop  r15 \n\t"
            "pop  r16 \n\t"
            "pop  r17 \n\t"
            "pop  r18 \n\t"
            "pop  r19 \n\t"
            "pop  r20 \n\t"
            "pop  r21 \n\t"
            "pop  r22 \n\t"
            "pop  r23 \n\t"
            "pop  r24 \n\t"
            "pop  r25 \n\t"
            "pop  r26 \n\t"
            "pop  r27 \n\t"
            "pop  r28 \n\t"
            "pop  r29 \n\t"
            "pop  r30 \n\t"
            "pop  r31 \n\t"
            "brtc 2f \n\t" //if the T flag is clear, do the non-interrupt enable return
            "reti \n\t"
            "2: \n\t"
            "ret \n\t"
            "" ::
            [_SREG_] "i" _SFR_IO_ADDR(SREG),
            [_I_] "i" SREG_I,
            [_T_] "i" SREG_T,
            [_nI_MASK_] "i" (~(1 << SREG_I)),
            [_SPL_] "i" _SFR_IO_ADDR(SPL),
            [_SPH_] "i" _SFR_IO_ADDR(SPH),
            [_next_task_] "r" (task));
}

