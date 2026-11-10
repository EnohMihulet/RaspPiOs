#include <stdint.h>
#include "et.h"
#include "page_allocator.h"
#include "usart.h"
#include "handle_exceptions.h"
#include "memmap.h"
#include "alloc.h"

extern char __kernel_start[];
extern char __kernel_end[];

static memmap_map_t memmap;

int main(int argc, char *argv[]);

void setup(void) {}
void teardown(void) {}

TEST_GROUP("test memory") {
	TEST("allocated pages retain independent patterns") {
		uint64_t free_count = page_free_count();
		uint64_t total_count = page_total_count();
		VERIFY(total_count == free_count && free_count != 0);
		VERIFY(free_count >= 2);
		uint64_t addr1, addr2;
		VERIFY((addr1 = page_alloc()) != INVALID_PHYS_ADDR);
		VERIFY((addr2 = page_alloc()) != INVALID_PHYS_ADDR);
		VERIFY(addr1 != addr2);
		VERIFY((addr1 & (PAGE_SIZE - 1)) == 0);
		VERIFY((addr2 & (PAGE_SIZE - 1)) == 0);
		VERIFY(free_count - 2 == page_free_count());
		VERIFY(total_count == page_total_count());

		/* Physical addresses are directly accessible while the MMU is off. */
		volatile uint64_t* first = (volatile uint64_t*)(uintptr_t)addr1;
		volatile uint64_t* second = (volatile uint64_t*)(uintptr_t)addr2;
		for (size_t i = 0; i < PAGE_SIZE / sizeof(uint64_t); i++) {
			first[i] = UINT64_C(0xaaaaaaaaaaaaaaaa) ^ i;
		}
		for (size_t i = 0; i < PAGE_SIZE / sizeof(uint64_t); i++) {
			second[i] = UINT64_C(0x5555555555555555) ^ i;
		}
		__asm__ volatile("dsb sy" ::: "memory");
		for (size_t i = 0; i < PAGE_SIZE / sizeof(uint64_t); i++) {
			VERIFY(first[i] == (UINT64_C(0xaaaaaaaaaaaaaaaa) ^ i));
			VERIFY(second[i] == (UINT64_C(0x5555555555555555) ^ i));
		}

		VERIFY(page_free(addr1) == 0);
		VERIFY(page_free(addr2) == 0);
		VERIFY(page_free_count() == free_count);
		VERIFY(page_total_count() == total_count);
	}
}

int kernel_main(uint64_t dtb_address) {
	disable_irqs();
	uart_init();
	alloc_init();
	if (memmap_init(&memmap, (const void*)(uintptr_t)dtb_address, dtb_address) != 0) panic("memmap_init() failed.\r\n");
	if (memmap_add_ram(&memmap) != 0) panic("memmap_add_ram() failed.\r\n");
	if (memmap_reserve(&memmap, (uintptr_t)__kernel_start, (uintptr_t)__kernel_end) != 0) panic("memmap_reserve() failed.\r\n");
	if (memmap_finalize(&memmap) != 0) panic("memmap_finalize() failed.\r\n");
	if (page_allocator_init(&memmap) != 0) panic("page_allocator_init() failed.\r\n");
	main(0, NULL);
	for (;;) {
		__asm__ volatile("wfe");
	}
}
