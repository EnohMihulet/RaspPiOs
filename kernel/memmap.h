#pragma once

#include <stddef.h>
#include <stdint.h>

#include "fdt.h"

#define DYNAMIC_ARRAY_NO_STDLIB
#include "dynamic_array.h"

#define MEMMAP_MAX_REGIONS 128

typedef enum {
	MEM_USABLE,
	MEM_RESERVED,
	MEM_KERNEL,
	MEM_RECLAIMABLE
} memmap_type_t;

typedef struct {
	uint64_t base;
	uint64_t size;
	memmap_type_t type;
} memmap_region_t;

DYNAMIC_ARRAY_DECLARE(memmap_regions, memmap_region_t)

typedef struct memmap_map_t {
	memmap_regions_dynamic_array regions;
} memmap_map_t;

int memmap_init(memmap_map_t* map, void* dtb);
int memmap_add_ram(memmap_map_t* map);
int memmap_reserve(memmap_map_t* map);
int memmap_finalize(memmap_map_t* map);
int memmap_next_usable();
