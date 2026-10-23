#include "alloc.h"
#include "thread.h"
#include "handle_exceptions.h"
#include "printk.h"
#include "string.h"
#include "scheduler.h"
#include <stdint.h>

#define RING_BUFFER_NO_STDLIB
#include "ring_buffer.h"

// Ordinary runnable threads and their stacks. Indexed by id.
volatile thread_t threads[MAX_THREADS];
static uint8_t thread_stacks[MAX_THREADS][STACK_SIZE] __attribute__((aligned(16)));

// Available ids for ordinary threads.
RING_BUFFER_DECLARE(thread_idq, thread_id_t)
RING_BUFFER_DEFINE(thread_idq, thread_id_t)
thread_idq_ring_buffer thread_idq;

// Container allocator for use in /ccontainer containers.
static container_allocator allocator = {
	.allocate = freelist_container_allocate,
	.deallocate = freelist_container_deallocate,
	.context = &_allocator,
};

// Default thread and its stack. Only runs when no ordinary threads are awaiting execution.
volatile thread_t default_thread;
static uint8_t default_thread_stack[STACK_SIZE] __attribute__((aligned(16)));
const thread_id_t THREAD_ID_DEFAULT_THREAD = (uint32_t)(-1);

// The thread currently executing
volatile thread_t* current_thread;

void default_thread_entry(void* args) {
	(void)args;
	for (;;) __asm__ volatile("wfe");
}

void default_thread_init() {
	default_thread.id = THREAD_ID_DEFAULT_THREAD;
	
	uint64_t top = ((uint64_t)(default_thread_stack + STACK_SIZE) & ~0xFull);
	top -= sizeof(exception_frame_t);

	exception_frame_t* frame = (exception_frame_t*)top;
	memset((void*)frame, 0, sizeof(exception_frame_t));

	default_thread.frame = frame;

	frame->sp = top;
	frame->elr_el1 = (uint64_t)thread_bootstrap;
	frame->x[0] = (uint64_t)default_thread_entry;
	frame->x[1] = (uint64_t)NULL;
	frame->x[30] = (uint64_t)thread_exit;
	frame->spsr_el1 = 0x5; // Return to EL1h & use SP_EL1

	default_thread.state = THREAD_READY;
}

void threads_init() {
	if (thread_idq_init(&thread_idq, &allocator) == 1)
		panic("thread_idq_init() failed.\r\n");

	if (thread_idq_reserve(&thread_idq, MAX_THREADS) == 1)
		panic("thread_idq_reserve() failed.\r\n");

	for (thread_id_t i = 0; i < MAX_THREADS; i++)
		thread_idq_push_back(&thread_idq, i);

	default_thread_init();
	default_thread.state = THREAD_RUNNING;
	current_thread = &default_thread;
}

void thread_bootstrap(void (*entry)(void* args), void* args) {

	entry(args);

	thread_exit();
}

exception_frame_t* next_thread(exception_frame_t* frame) {

	current_thread->frame = frame;

	scheduler_cleanup();

	thread_t* next = scheduler_next();

	next->state = THREAD_RUNNING;
	current_thread = next;

	return next->frame;
}

int thread_create(void (*entry)(void*), void* args, thread_id_t* id_out) {
	thread_id_t id;

	if (thread_idq_pop_front(&thread_idq, &id) == 1) {
		return 1;
	}

	thread_t* thread = (thread_t*)&threads[id];
	thread->id = id;

	thread->state = THREAD_BLOCKED;

	uint64_t top = ((uint64_t)(thread_stacks[id] + STACK_SIZE) & ~0xFull);
	top -= sizeof(exception_frame_t);

	exception_frame_t* frame = (exception_frame_t*)top;
	memset((void*)frame, 0, sizeof(exception_frame_t));

	thread->frame = frame;

	frame->sp = top;
	frame->elr_el1 = (uint64_t)thread_bootstrap;
	frame->x[0] = (uint64_t)entry;
	frame->x[1] = (uint64_t)args;
	frame->x[30] = (uint64_t)thread_exit;
	frame->spsr_el1 = 0x5; // Return to EL1h & use SP_EL1

	thread->state = THREAD_READY;

	if (scheduler_add(thread) == 1) {
		memset((void*)thread, 0, sizeof(thread_t));
		return 1;
	}

	*id_out = id;

	return 0;
}

int thread_delete(thread_id_t id) {
	if (id >= MAX_THREADS) return 1;

	thread_t* thread = (thread_t*)&threads[id];

	if (thread->state == THREAD_DEAD) return 1;

	scheduler_delete(thread);
	
	return 0;
}

int thread_wakeup(thread_id_t id) {
	if (id >= MAX_THREADS) return 1;

	thread_t* thread = (thread_t*)&threads[id];

	if (thread->state != THREAD_BLOCKED) return 1;

	scheduler_wakeup(thread);

	return 0;
}

void thread_reclaim(thread_t* thread) {
	if (thread == NULL) return;

	uint32_t id = thread->id;
	thread_idq_push_back(&thread_idq, id);
	kprintf("Reclaimed %u.\r\n", id);

	memset(thread_stacks[id], 0, STACK_SIZE);
}

void thread_yield(void) {
	__asm__ volatile("svc #0" ::: "memory");
}

_Noreturn void thread_exit(void) {
	__asm__ volatile("svc #1" ::: "memory");

	panic("thread_exit() returned.\r\n");
}

void thread_block(void) {
	__asm__ volatile("svc #2" ::: "memory");
}

