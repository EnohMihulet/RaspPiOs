#pragma once

#include <stdint.h>

extern volatile uint64_t timer_ticks_ms;

void timer_init(void);

uint64_t timer_get_frequency(void);
uint64_t timer_get_count(void);

void timer_set_tval(uint32_t ticks);
void timer_set_ctl(uint64_t value);

uint64_t timer_get_ctl(void);

void delay_ms(uint64_t ms);

uint64_t get_now();

uint64_t ms_to_counter(uint64_t ms);
uint64_t s_to_counter(uint64_t s);

void timer_rearm(void);

