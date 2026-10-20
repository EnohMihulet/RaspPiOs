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
	THREAD_READY,
	THREAD_RUNNING,
	THREAD_BLOCKED,
} thread_state_t;

typedef struct {
	uint32_t id;

	thread_state_t state;

	exception_frame_t* frame;

	uint8_t* stack;
	uint64_t stack_size;
} thread_t;

extern volatile thread_t threads[MAX_THREADS];
extern volatile thread_t* current_thread;

void context_switch(context_t* old, context_t* new);

void yield(void);

void threads_init(void);

exception_frame_t* next_thread(exception_frame_t* frame);

void thread_do_something();

void thread_exit(void);
