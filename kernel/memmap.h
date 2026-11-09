#pragma once

#include <stddef.h>
#include <stdint.h>

#include "fdt.h"

#define DYNAMIC_ARRAY_NO_STDLIB
#include "dynamic_array.h"

typedef enum {
	MEM_USABLE,
	MEM_RESERVED,
	MEM_KERNEL,
	MEM_RECLAIMABLE
} memmap_type_t;

typedef enum {
	MEM_ITER_SUCCESS = 0,
	MEM_ITER_ERROR = 1,
	MEM_ITER_NO_MORE_REGIONS = 2,
} memmap_iter_result_t;

typedef struct {
	uint64_t base;
	uint64_t size;
	memmap_type_t type;
} memmap_region_t;

DYNAMIC_ARRAY_DECLARE(memmap_regions, memmap_region_t)

typedef struct memmap_map_t {
	memmap_regions_dynamic_array regions;
} memmap_map_t;

typedef struct {
	size_t idx;
	const memmap_map_t* map;
} memmap_iter_t;

/* dtb is readable storage; dtb_phys_addr is its physical reservation address. */
int memmap_init(memmap_map_t* map, const void* dtb, uint64_t dtb_phys_addr);
int memmap_add_ram(memmap_map_t* map);
/* Kernel bounds are [start, end); equal bounds omit the kernel reservation. */
int memmap_reserve(memmap_map_t* map, uint64_t kernel_start, uint64_t kernel_end);
int memmap_finalize(memmap_map_t* map);

/* Iterate after finalization. The map must remain alive and unmodified.
 * Empty maps initialize successfully; next reports exhaustion. */
memmap_iter_result_t memmap_iter_init(const memmap_map_t* map, memmap_iter_t* iter);
memmap_iter_result_t memmap_iter_next(memmap_iter_t* iter, memmap_region_t* out);

bool memmap_is_reserved(memmap_map_t* map, uint64_t addr);
