#include "thread.h"
#include "handle_exceptions.h"
#include "string.h"
#include "printk.h"
#include "scheduler.h"

static uint8_t thread_stacks[MAX_THREADS][STACK_SIZE] __attribute__((aligned(16)));

volatile thread_t threads[MAX_THREADS];
volatile thread_t* current_thread;


void threads_init() {
	for (uint32_t i = 0; i < MAX_THREADS; i++) {
		thread_t* thread = &threads[i];

		thread->id = i; thread->state = THREAD_BLOCKED;

		uint64_t top = ((uint64_t)(thread_stacks[i] + STACK_SIZE) & ~0xFull);
		top -= sizeof(exception_frame_t);

		exception_frame_t* frame = (exception_frame_t*)top;
		memset((void*)frame, 0, sizeof(exception_frame_t));

		frame->elr_el1 = (uint64_t)thread_do_something;
		frame->spsr_el1 = 0x5; // Return to EL1h & use SP_EL1
		frame->x[30] = (uint64_t)thread_exit;

		thread->frame = frame;
		thread->frame->sp = top;

		thread->state = THREAD_READY;
	}
}

exception_frame_t* next_thread(exception_frame_t* frame) {
		current_thread->frame = frame;
		current_thread->state = THREAD_READY;

		thread_t* next = scheduler_next();

		next->state = THREAD_RUNNING;
		current_thread = next;

		return next->frame;
}

int thread_create(void (*entry)(void*), void* args) {
	return 0;
}

void thread_do_something() {
	switch (current_thread->id) {
		case 0: while (1) { kprintf("00000000\n"); }
		case 1: while (1) { kprintf("11111111\n"); }
		case 2: while (1) { kprintf("22222222\n"); }
		case 3: while (1) { kprintf("33333333\n"); }
		case 4: while (1) { kprintf("44444444\n"); }
		case 5: while (1) { kprintf("55555555\n"); }
		case 6: while (1) { kprintf("66666666\n"); }
		case 7: while (1) { kprintf("77777777\n"); }
		default: panic("Why?\r\n");
	}
}

void thread_exit(void) {
	kprintf("Thread returned!\n");
	
	for (;;)
		__asm__ volatile("wfe");
}
