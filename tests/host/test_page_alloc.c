#include "align.h"
#include "et.h"
#include "page_allocator.h"

#define FREE_LIST_IMPLEMENTATION
#include "alloc.h"
#include "memmap.h"
#include "fdt_builder.h"

freelist_allocator _allocator;
_Alignas(16) uint8_t _allocator_memory[ALLOCATOR_ARENA_SIZE];

static bool allocator_ready;
static memmap_map_t map;
static uint64_t kernel_start = 0x0;
static uint64_t kernel_end = 0x24000;

void setup(void) {
	map = (memmap_map_t){0};
	allocator_ready = freelist_init(&_allocator, _allocator_memory, sizeof(_allocator_memory)) == FREELIST_OK;
}

static bool init_dtb(const test_dtb_spec_t* spec) {
	return allocator_ready && test_dtb_build(spec) != 0;
}
/*
[TYPE: USABLE ; BASE: 0xbf000; SIZE:0x2ef2d000]
[TYPE: USABLE ; BASE: 0x2f000000; SIZE:0x10c00000]
[TYPE: USABLE ; BASE: 0x40000000; SIZE:0x1c0000000]
*/

static bool init_map(void) {
	const test_range_t ram[] = {
		{0xbf000, 0x2ef2d000},
		{0x2f000000, 0x10c00000},
	};
	const test_dtb_spec_t spec = {
		.ram = ram,
		.ram_count = sizeof(ram) / sizeof(ram[0]),
	};
	if (!init_dtb(&spec)) return false;
	if (memmap_init(&map, buffer, 0) != 0) return false;
	if (memmap_add_ram(&map) != 0) return false;
	if (memmap_reserve(&map, kernel_start, kernel_end) != 0) return false;
	return memmap_finalize(&map) == 0;
}

static bool init_page_alloc() {
	if (!init_map()) return false;
	return page_allocator_init(&map) == 0;
}

void teardown(){}

TEST_GROUP("test page allocator") {
	TEST("physical page zero is allocatable and can be freed") {
		const test_range_t ram[] = {{0, PAGE_SIZE}};
		const test_dtb_spec_t spec = {.ram = ram, .ram_count = 1};
		VERIFY(init_dtb(&spec));
		VERIFY(memmap_init(&map, buffer, MAX_PHYS_ADDR) == 0);
		VERIFY(memmap_add_ram(&map) == 0);
		VERIFY(memmap_finalize(&map) == 0);
		VERIFY(page_allocator_init(&map) == 0);
		VERIFY(page_total_count() == 1 && page_free_count() == 1);
		VERIFY(page_alloc() == 0);
		VERIFY(page_alloc() == INVALID_PHYS_ADDR);
		VERIFY(page_free(0) == 0);
		VERIFY(page_alloc() == 0);
	}

	TEST("RAM ending at or crossing bitmap limit retains the last page") {
		for (size_t extra = 0; extra < 2; extra++) {
			const test_range_t ram[] = {
				{MAX_PHYS_ADDR - PAGE_SIZE, PAGE_SIZE * (1 + extra)},
			};
			const test_dtb_spec_t spec = {.ram = ram, .ram_count = 1};
			VERIFY(init_dtb(&spec));
			VERIFY(memmap_init(&map, buffer, 0) == 0);
			VERIFY(memmap_add_ram(&map) == 0);
			VERIFY(memmap_finalize(&map) == 0);
			VERIFY(page_allocator_init(&map) == 0);
			VERIFY(page_total_count() == 1 && page_free_count() == 1);
			VERIFY(page_alloc() == MAX_PHYS_ADDR - PAGE_SIZE);
			VERIFY(page_alloc() == INVALID_PHYS_ADDR);
			VERIFY(page_free(MAX_PHYS_ADDR) == 1);
			VERIFY(page_free_count() == 0);
			VERIFY(page_free(MAX_PHYS_ADDR - PAGE_SIZE) == 0);
			VERIFY(page_alloc() == MAX_PHYS_ADDR - PAGE_SIZE);
			memmap_regions_destroy(&map.regions);
		}
	}

	TEST("allocate and free a page") {
		VERIFY(init_page_alloc());
		uint64_t free_count = page_free_count();
		uint64_t total_count = page_total_count();
		phys_addr_t addr;
		VERIFY((addr = page_alloc()) != INVALID_PHYS_ADDR);
		VERIFY(free_count - 1 == page_free_count());
		VERIFY(total_count == page_total_count());
		VERIFY(page_free(addr) == 0);
		VERIFY(free_count == page_free_count());
		VERIFY(total_count == page_total_count());
	}

	TEST("free unaligned address") {
		VERIFY(init_page_alloc());
		uint64_t free_count = page_free_count();
		uint64_t total_count = page_total_count();
		phys_addr_t addr;
		VERIFY((addr = page_alloc()) != INVALID_PHYS_ADDR);
		VERIFY(free_count - 1 == page_free_count());
		VERIFY(total_count == page_total_count());
		VERIFY(page_free(addr + 1) == 1);
		VERIFY(free_count - 1 == page_free_count());
		VERIFY(total_count == page_total_count());
	}

	TEST("free reserved address") {
		VERIFY(init_page_alloc());
		uint64_t free_count = page_free_count();
		uint64_t total_count = page_total_count();
		phys_addr_t addr = u64_align_down(kernel_end - 1, PAGE_SIZE);
		VERIFY(page_free(addr) == 1);
		VERIFY(free_count == page_free_count());
		VERIFY(total_count == page_total_count());
	}

	TEST("double free") {
		VERIFY(init_page_alloc());
		uint64_t free_count = page_free_count();
		uint64_t total_count = page_total_count();
		phys_addr_t addr;
		VERIFY((addr = page_alloc()) != INVALID_PHYS_ADDR);
		VERIFY(free_count - 1 == page_free_count());
		VERIFY(total_count == page_total_count());
		VERIFY(page_free(addr) == 0);
		VERIFY(page_free(addr) == 1);
		VERIFY(free_count == page_free_count());
		VERIFY(total_count == page_total_count());
	}

	TEST("reserved page inside RAM cannot be freed or allocated") {
		const test_range_t ram[] = {{0x1000, 0x3000}};
		const test_range_t reserved[] = {{0x2000, 0x1000}};
		const test_dtb_spec_t spec = {
			.ram = ram, .ram_count = 1,
			.reserved_memory = reserved, .reserved_memory_count = 1,
		};
		VERIFY(init_dtb(&spec));
		VERIFY(memmap_init(&map, buffer, 0) == 0);
		VERIFY(memmap_add_ram(&map) == 0);
		VERIFY(memmap_reserve(&map, 0, 0) == 0);
		VERIFY(memmap_finalize(&map) == 0);
		VERIFY(page_allocator_init(&map) == 0);
		VERIFY(page_free_count() == 2);
		VERIFY(page_free(0x2000) == 1);
		VERIFY(page_free(0x1000) == 1); /* Usable, but never allocated. */
		VERIFY(page_free_count() == 2);
		phys_addr_t first = page_alloc();
		phys_addr_t second = page_alloc();
		VERIFY((first == 0x1000 && second == 0x3000) ||
			(first == 0x3000 && second == 0x1000));
		VERIFY(page_alloc() == INVALID_PHYS_ADDR);
		VERIFY(page_free(0x2000) == 1);
		VERIFY(page_free_count() == 0);
		VERIFY(page_alloc() == INVALID_PHYS_ADDR);
		VERIFY(page_free(first) == 0);
		VERIFY(page_free_count() == 1);
		VERIFY(page_alloc() == first);
		VERIFY(page_free_count() == 0);
		VERIFY(page_alloc() == INVALID_PHYS_ADDR);
	}

	TEST("allocate all memory") {
		VERIFY(init_page_alloc());
		uint64_t free_count = page_free_count();
		uint64_t total_count = page_total_count();
		for (uint64_t i = 0; i < free_count; i++) {
			VERIFY(page_alloc() != INVALID_PHYS_ADDR);
		}
		VERIFY(page_free_count() == 0);
		VERIFY(total_count == page_total_count());

		VERIFY(page_alloc() == INVALID_PHYS_ADDR);
		VERIFY(page_free_count() == 0);
		VERIFY(total_count == page_total_count());
	}

	TEST("initialization resets total and free page counts") {
		const test_range_t ram[] = {{0x1000, 0x3000}};
		const test_dtb_spec_t spec = {.ram = ram, .ram_count = 1};
		VERIFY(init_dtb(&spec));
		VERIFY(memmap_init(&map, buffer, 0) == 0);
		VERIFY(memmap_add_ram(&map) == 0);
		VERIFY(memmap_finalize(&map) == 0);
		for (size_t pass = 0; pass < 2; pass++) {
			VERIFY(page_allocator_init(&map) == 0);
			VERIFY(page_total_count() == 3);
			VERIFY(page_free_count() == 3);
		}
	}
}
