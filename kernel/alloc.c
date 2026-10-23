#include "handle_exceptions.h"

#define FREE_LIST_IMPLEMENTATION
#include "alloc.h"

freelist_allocator _allocator;
uint8_t _allocator_memory[ALLOCATOR_ARENA_SIZE];

void alloc_init() {
	if (freelist_init(&_allocator, _allocator_memory, ALLOCATOR_ARENA_SIZE) == 1) 
		panic("freeelist_init() failed.\r\n");
}
