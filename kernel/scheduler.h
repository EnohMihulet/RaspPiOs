#pragma once

#include "thread.h"

void scheduler_init();

thread_t* scheduler_next(void);
