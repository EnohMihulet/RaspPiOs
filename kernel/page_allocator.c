#include "page_allocator.h"

extern int64_t __kernel_start;
extern int64_t __kernel_end;

uint64_t kernel_start;
uint64_t kernel_end;

void reserve_region(uint64_t start, uint64_t size) {
}

void page_allocator_init() {
}

void reserve_region(uint64_t start, uint64_t size);

void* page_alloc(void) {
}

void page_free(void* page) {
}
