#include <stdint.h>

#include "handle_exceptions.h"
#include "printk.h"
#include "gic.h"
#include "thread.h"
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

exception_frame_t* sync_handler(exception_frame_t* frame) {
	exception_frame_t* return_frame = frame;
	uint32_t ec = (uint32_t)((frame->esr_el1 >> 26) & 0x3fu);

	if (ec == 0x15) {
		uint32_t op = (uint32_t)(frame->esr_el1 & 0xffffu);
		
		switch (op) {
		case 0:
			return_frame = next_thread(frame);
			break;
		case 1:
			current_thread->state = THREAD_DEAD;
			return_frame = next_thread(frame);
			break;
		case 2:
			current_thread->state = THREAD_BLOCKED;
			return_frame = next_thread(frame);
			break;
		default:
			panic("Invalid svc operand.\r\n");
		}
	}
	else {
		kprintf("Synchronous Exception: EC=0x%x\r\n", ec);
		dump_exception_frame(frame);
		panic("Synchronous Exception\r\n");
	}

	return return_frame;
}

exception_frame_t* irq_handler(exception_frame_t* frame) {
	exception_frame_t* return_frame = frame;
	uint32_t iar = gic_acknowledge_irq();
	uint32_t irq = iar & GICC_IAR_INTID_MASK;

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

void enable_irqs(void) {
	__asm__ volatile("msr daifclr, #2" ::: "memory");
}

void disable_irqs(void) {
	__asm__ volatile("msr daifset, #2" ::: "memory");
}
