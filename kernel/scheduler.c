#include "scheduler.h"
#include "thread.h"

#define FREE_LIST_IMPLEMENTATION
#include "freelist.h"

#define RING_BUFFER_NO_STDLIB
#include "ring_buffer.h"
typedef thread_t* thread_t_ptr;
RING_BUFFER_DECLARE(threadq, thread_t_ptr)
RING_BUFFER_DEFINE(threadq, thread_t_ptr)

freelist_allocator _allocator;
static uint8_t* _allocator_memory[1024];

container_allocator allocator = {
	.allocate = freelist_container_allocate,
	.deallocate = freelist_container_deallocate,
	.context = &_allocator,
};

threadq_ring_buffer thread_queue;

void scheduler_init() {

	if (freelist_init(&_allocator, _allocator_memory, 1024) != FREELIST_OK) {
		panic("freelist_init() failed.\r\n");
	}

	if (threadq_init(&thread_queue, &allocator) != 0) {
		panic("threadq_init() failed.\r\n");
	}

	for (uint32_t i = 0; i < MAX_THREADS; i++) {
		if (threadq_push_back(&thread_queue, (thread_t_ptr)&threads[i]) != 0) panic("threadq_push_back() failed.\r\n");
	}
}

thread_t* scheduler_next(void) {
	if (threadq_is_empty(&thread_queue)) panic("thread queue is empty.\r\n");

	thread_t* next;
	threadq_pop_front(&thread_queue, &next);
	if (next == NULL) panic("threadq_pop_front() next is null.\r\n");
	threadq_push_back(&thread_queue, (thread_t_ptr)current_thread);

	return next;
}
