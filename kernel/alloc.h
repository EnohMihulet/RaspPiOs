#pragma once

#include "freelist.h"

#define ALLOCATOR_ARENA_SIZE 2048

extern freelist_allocator _allocator;
extern uint8_t _allocator_memory[ALLOCATOR_ARENA_SIZE];

void alloc_init();

