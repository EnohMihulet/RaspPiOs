#include "thread.h"
#include "printk.h"

volatile uint8_t stack_a[STACK_SIZE] __attribute__((aligned(16)));
volatile uint8_t stack_b[STACK_SIZE] __attribute__((aligned(16)));

thread_t* current_thread;
volatile thread_t boot_thread;
volatile thread_t thread_a;
volatile thread_t thread_b;

void yield(void) {
	thread_t* old = current_thread;
	thread_t* next;

	if (current_thread == &thread_a) next = &thread_b;
	else next = &thread_a;

	current_thread = next;

	context_switch(&old->context, &next->context);
}

void boot_thread_init(void) {
	current_thread = &boot_thread;
	__asm__ volatile("mov %0, sp\n\t" : "=r"(boot_thread.context.sp));
	boot_thread.context.sp &= ~0xFull;
	boot_thread.id = 0;
}

void thread_a_init(void) {
	thread_a.context.sp = (uint64_t)(stack_a + STACK_SIZE);
	thread_a.context.sp &= ~0xFull;
	thread_a.context.x30 = (uint64_t)thread_a_test;
	thread_a.id = 1;
}

void thread_b_init(void) {
	thread_b.context.sp = (uint64_t)(stack_b + STACK_SIZE);
	thread_b.context.sp &= ~0xFull;
	thread_b.context.x30 = (uint64_t)thread_b_test;
	thread_b.id = 2;
}

void thread_a_test(void) {
	for (;;) {
		kprintf("A");
		yield();
	}
}

void thread_b_test(void) {
	for (;;) {
		kprintf("B");
		yield();
	}
}
