#include <stdint.h>

#include "usart.h"
#include "gic.h"
#include "timer.h"
#include "printk.h"
#include "handle_exceptions.h"

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

static inline void enable_irqs(void) {
	__asm__ volatile("msr daifclr, #2" ::: "memory");
}

static inline void dsb_sy(void) {
	__asm__ volatile("dsb sy\n\tisb" ::: "memory");
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

	enable_irqs();

	for (;;) {
		__asm__ volatile("wfe");
	}
	
}
