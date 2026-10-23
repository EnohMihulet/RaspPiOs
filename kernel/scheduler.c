#include "alloc.h"
#include "scheduler.h"
#include "gic.h"
#include "handle_exceptions.h"
#include "printk.h"
#include "thread.h"

#define RING_BUFFER_NO_STDLIB
#include "ring_buffer.h"

typedef thread_t* thread_t_ptr;
RING_BUFFER_DECLARE(threadq, thread_t_ptr)
RING_BUFFER_DEFINE(threadq, thread_t_ptr)

threadq_ring_buffer ready_threadq;
threadq_ring_buffer cleanup_threadq;

volatile bool SCHEDULER_STOPPED = false;

static container_allocator allocator = {
	.allocate = freelist_container_allocate,
	.deallocate = freelist_container_deallocate,
	.context = &_allocator,
};

void scheduler_init(void) {

	disable_irqs();

	if (threadq_init(&ready_threadq, &allocator) != 0) {
		panic("threadq_init() failed.\r\n");
	}

	if (threadq_init(&cleanup_threadq, &allocator) != 0) {
		panic("threadq_init() failed.\r\n");
	}

	enable_irqs();
}

thread_t* scheduler_next(void) {

	thread_t* thread = (thread_t*)current_thread;

	if (thread->id != THREAD_ID_DEFAULT_THREAD && thread->state == THREAD_RUNNING) {
		thread->state = THREAD_READY;
		threadq_push_back(&ready_threadq, thread);
	}
	else if (thread->state == THREAD_DEAD) {
		scheduler_delete(thread);
	}
	else if (thread->state == THREAD_READY) {
		kprintf("WHY: current_thread is ready\r\n");
	}
	
	thread_t_ptr next;
	if (threadq_pop_front(&ready_threadq, &next) == 1) {
		next = (thread_t*)&default_thread;
	}

	return next;
}

int scheduler_add(thread_t* thread) {

	disable_irqs();

	if (thread == NULL) return 1;

	if (threadq_push_back(&ready_threadq, (thread_t_ptr)thread) == 1) return 1;

	enable_irqs();

	return 0;
}

static int scheduler_add_to_ready(thread_t* thread) {
	if (thread == NULL) return 1;

	threadq_push_back(&ready_threadq, (thread_t_ptr)thread);

	return 0;
}

static int scheduler_remove_from_ready(thread_t* thread) {
	if (thread == NULL) return 1;

	uint32_t index = 0;
	threadq_iter iter = threadq_iter_begin(&ready_threadq);
	const thread_t_ptr* out = NULL;
	while (threadq_iter_next(&iter, &out)) {
		if (*out == thread) {
			if (threadq_remove(&ready_threadq, index, NULL) == 1) {
				panic("Failed to remove thread from ready_threadq.\r\n");
			}
			break;
		}

		index += 1;

	}

	return 0;
}

static int scheduler_add_to_cleanup(thread_t* thread) {
	if (thread == NULL) return 1;

	if (threadq_push_back(&cleanup_threadq, (thread_t_ptr)thread) == 1) {
		panic("Failed to push thread into cleanup queue.\r\n");
	}

	return 0;
}

int scheduler_wakeup(thread_t* thread) {
	if (thread == NULL) return 1;

	thread->state = THREAD_READY;

	scheduler_add_to_ready(thread);

	return 0;
}

int scheduler_delete(thread_t* thread) {
	if (thread == NULL) return 1;

	thread->state = THREAD_DEAD;

	if (thread == current_thread) {
		scheduler_add_to_cleanup(thread);
	}
	else {
		scheduler_remove_from_ready(thread);
		scheduler_add_to_cleanup(thread);
	}

	return 0;
}

void scheduler_cleanup(void) {
	thread_t* thread;
	bool curr_found = false;

	kprintf("Cleanup\r\n");
	while (threadq_pop_front(&cleanup_threadq, &thread) == 0) {
		if (thread == current_thread) {
			curr_found = true;
			continue;
		}
		thread_reclaim(thread);
	}

	if (curr_found) threadq_push_back(&cleanup_threadq, (thread_t*)current_thread);
}
