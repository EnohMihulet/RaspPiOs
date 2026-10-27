#pragma once

#include "stdbool.h"

#include "thread.h"

extern volatile uint64_t scheduler_delay_ms;
extern volatile uint64_t scheduler_deadline;

void scheduler_init(void);

thread_t* scheduler_next(void);

int scheduler_add(thread_t* thread);
int scheduler_delete(thread_t* thread);
int scheduler_wakeup(thread_t* thread);

void scheduler_cleanup(void);
