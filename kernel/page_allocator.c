#include <stdbool.h>

#include "handle_exceptions.h"
#include "align.h"
#include "memmap.h"
#include "string.h"
#include "page_allocator.h"

static uint8_t reserved_bitmap[BITMAP_SIZE];
static uint8_t allocated[BITMAP_SIZE];

static uint64_t next_free_pfn = MAX_PAGES;
static uint64_t free_page_count = 0;
static uint64_t total_page_count = 0;

static inline void page_mark_usable(uint64_t pfn) {
	reserved_bitmap[pfn / 8] &= ~(1U << (pfn % 8));
}

static inline void page_mark_reserved(uint64_t pfn) {
	reserved_bitmap[pfn / 8] |= (1U << (pfn % 8));
}

static inline void page_mark_allocated(uint64_t pfn) {
	allocated[pfn / 8] |= (1U << (pfn % 8));
}

static inline void page_mark_free(uint64_t pfn) {
	allocated[pfn / 8] &= ~(1U << (pfn % 8));
}

static inline bool page_is_reserved(uint64_t pfn) {
	return reserved_bitmap[pfn / 8] & (1U << (pfn % 8));
}

static inline bool page_is_allocated(uint64_t pfn) {
	return allocated[pfn / 8] & (1U << (pfn % 8));
}

static inline void page_mark_region_usable(uint64_t base, uint64_t size) {
	uint64_t start = u64_align_up(base, PAGE_SIZE);
	uint64_t end = u64_align_down(base + size, PAGE_SIZE);
	for (uint64_t addr = start; addr < end; addr += PAGE_SIZE) {
		page_mark_usable(addr >> 12);
	}
}

static inline void page_mark_region_reserved(uint64_t base, uint64_t size) {
	uint64_t start = u64_align_up(base, PAGE_SIZE);
	uint64_t end = u64_align_down(base + size, PAGE_SIZE);
	for (uint64_t addr = start; addr < end; addr += PAGE_SIZE) {
		page_mark_reserved(addr >> 12);
	}
}

static inline void page_mark_region_allocated(uint64_t base, uint64_t size) {
	uint64_t start = u64_align_up(base, PAGE_SIZE);
	uint64_t end = u64_align_down(base + size, PAGE_SIZE);
	for (uint64_t addr = start; addr < end; addr += PAGE_SIZE) {
		page_mark_allocated(addr >> 12);
	}
}

static inline void page_mark_region_free(uint64_t base, uint64_t size) {
	uint64_t start = u64_align_up(base, PAGE_SIZE);
	uint64_t end = u64_align_down(base + size, PAGE_SIZE);
	for (uint64_t addr = start; addr < end; addr += PAGE_SIZE) {
		page_mark_free(addr >> 12);
	}
}

static inline void page_mark_all_reserved() {
	memset(reserved_bitmap, 0xFF, sizeof(reserved_bitmap));
}

static inline void page_mark_all_allocated() {
	memset(allocated, 0xFF, BITMAP_SIZE);
}

static uint64_t page_next_free_pfn(uint64_t pfn) {
	if (pfn >= MAX_PAGES - 1) return MAX_PAGES;

	for (uint64_t next = pfn + 1; next < MAX_PAGES; next++) {
		if (!page_is_allocated(next) && !page_is_reserved(next)) {
			return next;
		}
	}
	return MAX_PAGES;
}

int page_allocator_init(memmap_map_t* map) {
	uint64_t flags = irq_save();
	int status = 1;
	if (map == NULL) goto done;

	next_free_pfn = MAX_PAGES;
	page_mark_all_allocated();
	page_mark_all_reserved();
	free_page_count = 0;
	total_page_count = 0;

	memmap_iter_t iter;
	if (memmap_iter_init(map, &iter) != MEM_ITER_SUCCESS) goto done;

	memmap_region_t usable;
	memmap_iter_result_t result;
	while ((result = memmap_iter_next(&iter, &usable)) == MEM_ITER_SUCCESS) {
		uint64_t start = u64_align_up(usable.base, PAGE_SIZE);
		uint64_t end = u64_align_down(usable.base + usable.size, PAGE_SIZE);
		end = end < PAGE_SIZE * MAX_PAGES ? end : PAGE_SIZE * MAX_PAGES;

		for (uint64_t addr = start; addr < end; addr += PAGE_SIZE) {
			page_mark_free(addr >> 12);
			page_mark_usable(addr >> 12);
			free_page_count += 1;
			total_page_count += 1;
		}
	}
	if (result != MEM_ITER_NO_MORE_REGIONS) {
		page_mark_all_allocated();
		page_mark_all_reserved();
		free_page_count = 0;
		total_page_count = 0;
		goto done;
	}

	next_free_pfn = 0;
	if (page_is_allocated(next_free_pfn) || page_is_reserved(next_free_pfn)) {
		if ((next_free_pfn = page_next_free_pfn(next_free_pfn)) == MAX_PAGES) {
			page_mark_all_allocated();
			page_mark_all_reserved();
			free_page_count = 0;
			total_page_count = 0;
			goto done;
		}
	}

	status = 0;
done:
	irq_restore(flags);
	return status;
}

phys_addr_t page_alloc(void) {
	uint64_t flags = irq_save();
	phys_addr_t addr = INVALID_PHYS_ADDR;

	if (next_free_pfn == MAX_PAGES) goto done;

	while (page_is_allocated(next_free_pfn) || page_is_reserved(next_free_pfn)) {
		next_free_pfn = page_next_free_pfn(next_free_pfn);
		if (next_free_pfn == MAX_PAGES) goto done;
	}

	uint64_t pfn = next_free_pfn;
	page_mark_allocated(pfn);
	next_free_pfn = page_next_free_pfn(next_free_pfn);
	free_page_count -= 1;
	addr = pfn << 12;

done:
	irq_restore(flags);

	return addr;
}

int page_free(phys_addr_t addr) {
	uint64_t flags = irq_save();
	int status = 1;

	if (addr & (PAGE_SIZE - 1)) {
		goto done;
	}
	
	uint64_t pfn = addr >> 12;
	
	if (pfn >= MAX_PAGES) {
		goto done;
	}

	if (page_is_reserved(pfn)) {
		goto done;
	}
	
	if (!page_is_allocated(pfn)) {
		goto done;
	}
	
	page_mark_free(pfn);
	if (pfn < next_free_pfn) next_free_pfn = pfn;
	free_page_count += 1;
	status = 0;

done:
	irq_restore(flags);

	return status;
}

uint64_t page_free_count(void) {
	uint64_t flags = irq_save();
	uint64_t count = free_page_count;
	irq_restore(flags);
	return count;
}

uint64_t page_total_count(void) {
	uint64_t flags = irq_save();
	uint64_t count = total_page_count;
	irq_restore(flags);
	return count;
}
