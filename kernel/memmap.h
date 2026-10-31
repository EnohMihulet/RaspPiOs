#pragma once

#include <stddef.h>
#include <stdint.h>

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

typedef struct {
    memmap_region_t regions[MEMMAP_MAX_REGIONS];
    size_t count;
} memmap_map_t;


/*
* COLLECT MEMORY INFORMATION - RAM ranges and reservation (and kernel reserved ranges)
* MERGE OVERLAPPING AND ADJACENT REGIONS
* SUBTRACT RESERVED REGIONS
* ALIGN RANGES TO PAGE BOUNDRIES
* EXPOSE FINALIZED MEMORY INFO
* */

void memmap_init(memmap_map_t* map);
void memmap_add_ram(memmap_map_t* map);
void memmap_reserve(memmap_map_t* map);
void memmap_finalize(memmap_map_t* map);
void memmap_next_usable();
