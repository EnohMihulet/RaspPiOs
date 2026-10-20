#include <stdint.h>

volatile uint64_t timer_ticks = 0;

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

void timer_rearm() {
	timer_ticks++;

	uint32_t freq = timer_get_frequency();

	timer_set_tval(freq);
}
