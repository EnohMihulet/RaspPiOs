#pragma once

#include <stdint.h>

#define STACK_SIZE 4096

extern volatile uint8_t stack_a[STACK_SIZE];
extern volatile uint8_t stack_b[STACK_SIZE];

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

typedef struct {
	context_t context;
	uint32_t id;
} thread_t;

volatile extern thread_t boot_thread;
volatile extern thread_t thread_a;
volatile extern thread_t thread_b;

void context_switch(context_t* old, context_t* new);

void yield(void);

void boot_thread_init(void);
void thread_a_init(void);
void thread_b_init(void);
void thread_a_test(void);
void thread_b_test(void);
