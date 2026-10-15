#include <stdint.h>

#include "handle_exceptions.h"
#include "printk.h"
#include "gic.h"
#include "timer.h"

volatile uint64_t timer_ticks = 0;

void dump_exception_frame(exception_frame* frame) {
	for (int i = 0; i < 31; i++) {
		kprintf("x%d = 0x%lx\r\n", i, frame->r[i]);
	}
	kprintf("sp   = 0x%lx\r\n", frame->sp);
	kprintf("elr  = 0x%lx\r\n", frame->elr_el1);
	kprintf("spsr = 0x%lx\r\n", frame->spsr_el1);
	kprintf("esr  = 0x%lx\r\n", frame->esr_el1);
	kprintf("far  = 0x%lx\r\n", frame->far_el1);
}

void sync_handler(exception_frame* frame) {
	uint32_t ec = (uint32_t)((frame->esr_el1 >> 26) & 0x3fu);

	kprintf("Synchronous Exception: EC=0x%x\r\n", ec);
	dump_exception_frame(frame);
	panic("Synchronous Exception\r\n");
}

void irq_handler(exception_frame* frame) {
	(void)frame;
	uint32_t iar = gic_acknowledge_irq();
	uint32_t irq = iar & GICC_IAR_INTID_MASK;

	if (irq >= 1020u) {
		return;
	}

	if (irq == GIC_IRQ_TIMER_PHYS_NS) {
		timer_ticks++;

		uint32_t freq = timer_get_frequency();

		timer_set_tval(freq);
	}

	gic_end_irq(iar);
}

_Noreturn void panic(const char* message) {
	__asm__ volatile("msr daifset, #0xf" ::: "memory");

	kprintf("PANIC: %s\r\n", message);

	for (;;) {
		__asm__ volatile("wfe");
	}
}
