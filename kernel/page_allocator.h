#pragma once

#include <stddef.h>
#include <stdint.h>

#include "memmap.h"

#define PAGE_SIZE	4096
#define MAX_PHYS_ADDR	0x200000000ULL
#define MAX_PAGES	(MAX_PHYS_ADDR / PAGE_SIZE)
#define BITMAP_SIZE	((MAX_PAGES + 7) / 8)

#define INVALID_PHYS_ADDR UINT64_MAX
typedef uint64_t phys_addr_t;

/* Requires a finalized map of sorted, non-overlapping usable ranges.
 * The map is read during initialization only and is not retained. */
int page_allocator_init(memmap_map_t* map);
phys_addr_t page_alloc(void);
int page_free(phys_addr_t page);

uint64_t page_free_count(void);
uint64_t page_total_count(void);
