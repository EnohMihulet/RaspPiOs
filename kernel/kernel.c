#include <stdint.h>

#include "usart.h"
#include "gic.h"
#include "timer.h"
#include "printk.h"
#include "thread.h"
#include "scheduler.h"
#include "alloc.h"

static inline uint64_t read_current_el(void) {
	uint64_t value;

	__asm__ volatile(
	"mrs %0, CurrentEL"
	: "=r" (value)
	);

	return value;
}

static inline uint64_t read_mpidr_el1(void) {
	uint64_t value;

	__asm__ volatile(
	"mrs %0, MPIDR_EL1"
	: "=r" (value)
	);

	return value;
}

static inline void dsb_sy(void) {
	__asm__ volatile("dsb sy\n\tisb" ::: "memory");
}

static inline void kernel_wfe(void* arg) {
	(void)arg;
	for (;;) {
		__asm__ volatile("wfe");
	}
}

static volatile uint32_t test_stage;

static inline void worker(void* arg) {
	bool exit_on_return = (uintptr_t)arg != 0;

	kprintf("worker: yield\r\n");
	thread_yield();

	kprintf("worker: block\r\n");
	test_stage = 1;
	thread_block();

	kprintf("worker: wake up\r\n");
	test_stage = 2;

	if (exit_on_return) {
		kprintf("worker: exit\r\n");
		return;
	}

	for (;;) thread_yield();
}

static void thread_test(void* arg) {
	(void)arg;
	
	for (uint32_t round = 0;; round++) {
		thread_id_t id;
		bool exit_on_return = round % 2;
		test_stage = 0;

		if (thread_create(worker, (void*)(uintptr_t)exit_on_return, &id) != 0) {
			panic("test: thread_create failed.\r\n");
		}

		kprintf("test: created thread %u.\r\n", id);

		while (test_stage < 1) thread_yield();

		while (thread_wakeup(id) != 0) thread_yield();
		
		while (test_stage < 2) thread_yield();

		if (!exit_on_return) {
			if (thread_delete(id) != 0) panic("test: thread_delete failed.\r\n");
			
			kprintf("test: deleted thread %u.\r\n", id);
		}

		for (uint32_t i = 0; i < 4; i++) thread_yield();
	}
}

int kernel_main(uint64_t dtb_address) {

	uart_init();

	kprintf("\r\nBoot successful!\r\n");

	kprintf("CPU information\r\n");
	kprintf("---------------\r\n");
	kprintf("DTB:       %p\r\n", (void*)(uintptr_t)dtb_address);
	
	kprintf("CurrentEL: 0x%llx\r\n", (unsigned long long)read_current_el());
	
	kprintf("MPIDR:     0x%llx\r\n", (unsigned long long)read_mpidr_el1());
	

	gic_init();

	uint32_t freq = timer_get_frequency();
	timer_set_tval(freq);
	timer_set_ctl(1);

	dsb_sy();

	alloc_init();
	
	kprintf("alloc_init() successful.\r\n");

	threads_init();

	kprintf("threads_init() successful.\r\n");

	enable_irqs();

	scheduler_init();

	kprintf("scheduler_init() successful.\r\n");

	thread_id_t test_id;
	if (thread_create(thread_test, NULL, &test_id) != 0) panic("test: could not create controller.\r\n");

	for (;;) {
		__asm__ volatile("wfe");
	}
}

