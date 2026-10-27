#include <stdint.h>

#include "timer.h"

volatile uint64_t timer_ticks_ms = 0;

void timer_init(void) {
	uint32_t freq = timer_get_frequency();
	timer_set_tval(freq / 1000);
	timer_set_ctl(1);
}

uint64_t timer_get_frequency(void) {
	uint64_t freq;
	__asm__ volatile("mrs %0, CNTFRQ_EL0" : "=r"(freq));
	return freq;
}

uint64_t timer_get_count(void) {
	uint64_t cnt;
	__asm__ volatile(
		"isb\n\t"
		"mrs %0, CNTPCT_EL0"
		: "=r"(cnt)
	);

	return cnt;
}

void timer_set_tval(uint32_t ticks) {
	__asm__ volatile(
		"msr CNTP_TVAL_EL0, %0\n\r"
		"isb"
		:
		: "r"((uint64_t)ticks)
	);
}

void timer_set_ctl(uint64_t value) {
	__asm__ volatile(
		"msr CNTP_CTL_EL0, %0\n\t" 
		"isb"
		:
		: "r"((uint64_t)value)
		: "memory"
	);
}

uint64_t timer_get_ctl(void) {
	uint64_t value;
	__asm__ volatile("mrs %0, CNTP_CTL_EL0" : "=r"(value));
	return value;
}

void delay_ms(uint64_t ms) {
	uint64_t freq = timer_get_frequency();

	uint64_t start = timer_get_count();
	uint64_t ticks = (freq * ms) / 1000;

	while ((timer_get_count() - start) < ticks) {}
}

uint64_t get_now() {
	return timer_get_count();
}

uint64_t ms_to_counter(uint64_t ms) {
	return (timer_get_frequency() * ms) / 1000;
}

uint64_t s_to_counter(uint64_t s) {
	return (timer_get_frequency() * s);
}

void timer_rearm(void) {
	timer_ticks_ms++;

	uint32_t freq = timer_get_frequency();

	timer_set_tval(freq / 1000);
}
