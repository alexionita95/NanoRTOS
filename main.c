/*
 * NanoRTOS.c
 *
 * Created: 12/4/2018 12:00:16 AM
 * Author : Alex Ionita
 */ 

#include <avr/io.h>
#include <stddef.h>
#include <stdlib.h>

#include "mutex.h"
#include "task.h"

volatile uint8_t delay_ms = 0;
Mutex m;


void change_task(void* unused)
{
while(1)
{

	mutexLock(&m);
	int r = rand()%50;
	delay_ms=r*100;
	taskSleep(delay_ms);
	mutexUnlock(&m);
}
}

void blink_task_white(void *unused) {
	while (1) {
		taskSleep(500);
		PORTB ^= _BV(PB3);
	}
}
void blink_task_red(void *unused) {
	while (1) {
		taskSleep(1000);
		PORTB ^= _BV(PB4);
	}
}
void blink_task_board(void *unused) {
	while (1) {
		mutexLock(&m);
		taskSleep(delay_ms);
		PORTB ^= _BV(PB5);
		mutexUnlock(&m);
	}
}


int main() {
	DDRB |= _BV(PB4);
	DDRB |= _BV(PB3);
	DDRB |= _BV(PB5);
	mutexInit(&m);
	taskInit();

	taskCreate(blink_task_white, NULL);
	taskCreate(blink_task_red, NULL);
	taskCreate(blink_task_board, NULL);
	taskCreate(change_task, NULL);
	
	taskStart();

	return 0; // Never reached
}

