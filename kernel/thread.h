#pragma once

#include <stdint.h>

#include "handle_exceptions.h"

#define MAX_THREADS 8
#define STACK_SIZE (16 * 1024)

typedef struct {
	uint64_t x19;
	uint64_t x20;
	uint64_t x21;
	uint64_t x22;
	uint64_t x23;
	uint64_t x24;
	uint64_t x25;
	uint64_t x26;
	uint64_t x27;
	uint64_t x28;

	uint64_t x29;
	uint64_t x30;

	uint64_t sp;
} context_t;

typedef enum {
	THREAD_DEAD = 0,
	THREAD_READY,
	THREAD_RUNNING,
	THREAD_BLOCKED,
} thread_state_t;

typedef uint32_t thread_id_t;

typedef struct {
	uint32_t id;

	thread_state_t state;

	exception_frame_t* frame;

	void (*entry)(void* args);
	void* args;
} thread_t;

extern volatile thread_t default_thread;
extern const thread_id_t THREAD_ID_DEFAULT_THREAD;
extern volatile thread_t* current_thread;

void threads_init(void);

void thread_bootstrap(void (*entry)(void* args), void* args);

exception_frame_t* next_thread(exception_frame_t* frame);

int thread_create(void (*entry)(void*), void* args, thread_id_t* id_out);
int thread_delete(thread_id_t id);
int thread_wakeup(thread_id_t id);

void thread_reclaim(thread_t* thread);

void thread_yield(void);
_Noreturn void thread_exit(void);
void thread_block(void);
