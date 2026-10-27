#include <stdint.h>

#include "handle_exceptions.h"
#include "printk.h"
#include "gic.h"
#include "scheduler.h"
#include "thread.h"
#include "timer.h"
#include "sync_handlers.h"

void dump_exception_frame(const exception_frame_t* frame) {
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
	if (frame == NULL) panic("sync_handler frame NULL");

	uint32_t ec = ESR_EC(frame->esr_el1);

	const exception_info_t* info = &exception_info[ec];
	if (info->handler == NULL) {
		kprintf("Unhandled synchronous exception: EC=0x%x\r\n", ec);
		kprintf("%s: %s\r\n",
			info->name ? info->name : "Unlisted EC",
			info->message ? info->message : "No description available");
		dump_exception_frame(frame);
		panic("Unhandled synchronous exception");
	}

	return info->handler(frame);
}

exception_frame_t* irq_handler(exception_frame_t* frame) {
	exception_frame_t* return_frame = frame;
	uint32_t iar = gic_acknowledge_irq();
	uint32_t irq = iar & GICC_IAR_INTID_MASK;

	if (irq == GIC_IRQ_TIMER_PHYS_NS) {
		uint64_t now = get_now();

		for (uint32_t i = 0; i < MAX_THREADS; i++) {
			thread_t* thread = (thread_t*)&threads[i];

			if (thread->state == THREAD_SLEEPING && now >= thread->wakeup_deadline) {
				scheduler_wakeup(thread);
			}
		}

		if (current_thread->id == THREAD_ID_DEFAULT_THREAD || now >= scheduler_deadline) {
			return_frame = next_thread(frame);
		}

		timer_rearm();
		
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
