#include <stdint.h>

#include "handle_exceptions.h"
#include "printk.h"
#include "gic.h"
#include "timer.h"
#include "scheduler.h"

void dump_exception_frame(exception_frame_t* frame) {
	for (int i = 0; i < 31; i++) {
		kprintf("x%d = 0x%lx\r\n", i, frame->x[i]);
	}
	kprintf("sp   = 0x%lx\r\n", frame->sp);
	kprintf("elr  = 0x%lx\r\n", frame->elr_el1);
	kprintf("spsr = 0x%lx\r\n", frame->spsr_el1);
	kprintf("esr  = 0x%lx\r\n", frame->esr_el1);
	kprintf("far  = 0x%lx\r\n", frame->far_el1);
}

void sync_handler(exception_frame_t* frame) {
	uint32_t ec = (uint32_t)((frame->esr_el1 >> 26) & 0x3fu);

	kprintf("Synchronous Exception: EC=0x%x\r\n", ec);
	dump_exception_frame(frame);
	panic("Synchronous Exception\r\n");
}

exception_frame_t* irq_handler(exception_frame_t* frame) {
	exception_frame_t* return_frame = frame;
	uint32_t iar = gic_acknowledge_irq();
	uint32_t irq = iar & GICC_IAR_INTID_MASK;

	// if (irq >= 1020u) {
	// 	return NULL;
	// }

	if (irq == GIC_IRQ_TIMER_PHYS_NS) {
		timer_rearm();

		return_frame = next_thread(frame);
	}

	gic_end_irq(iar);

	return return_frame;
}

_Noreturn void panic(const char* message) {
	__asm__ volatile("msr daifset, #0xf" ::: "memory");

	kprintf("PANIC: %s\r\n", message);

	for (;;) {
		__asm__ volatile("wfe");
	}
}
