#pragma once

#include <stddef.h>
#include <stdint.h>

#define PAGE_SIZE 4096

typedef struct page {
	size_t size;
	struct block* next_free;
	uint32_t magic;
	unsigned char free;
} page_t;

typedef struct {
	page_t* head;
	void* arena_start;
	size_t arena_size;
	uint32_t magic;
} page_allocator;

static inline uint64_t align_down(uint64_t x)
{
    return x & ~(PAGE_SIZE - 1);
}

static inline uint64_t align_up(uint64_t x)
{
    return (x + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
}

void page_allocator_init();

void reserve_region(uint64_t start, uint64_t size);

void* page_alloc(void);
void page_free(void* page);
