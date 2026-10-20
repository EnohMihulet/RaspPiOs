#pragma once

#include <stdint.h>

extern volatile uint64_t timer_ticks;

uint64_t timer_get_frequency(void);
uint64_t timer_get_count(void);
void timer_set_tval(uint32_t ticks);
void timer_set_ctl(uint64_t value);
uint64_t timer_get_ctl(void);
void delay_ms(uint64_t ms);
void timer_rearm();
