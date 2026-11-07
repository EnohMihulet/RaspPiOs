#include "memmap.h"
#include "dynamic_array.h"
#include "alloc.h"
#include "align.h"
#include "page_allocator.h"

#define FDT_IMPLEMENTATION
#include "fdt.h"

DYNAMIC_ARRAY_DEFINE(memmap_regions, memmap_region_t)

static size_t dtb_size;
static fdt_tree_t tree;

static container_allocator allocator = {
	.allocate = freelist_container_allocate,
	.deallocate = freelist_container_deallocate,
	.context = &_allocator
};

int memmap_init(memmap_map_t* map, void* dtb) {
	if (map == NULL || dtb == NULL) return 1;

	if (memmap_regions_init(&map->regions, &allocator) == 1) return 1;
	if (fdt_read_header_size(dtb, FDT_HEADER_SIZE, &dtb_size) != FDT_SUCCESS) return 1;
	if (fdt_init(&tree, dtb, dtb_size) != FDT_SUCCESS) return 1;

	return 0;
}

static int memmap_merge(memmap_map_t* map) {
	if (map->regions.size <= 1) return 0;

	memmap_region_t* curr, *next;
	size_t curr_idx = 0;
	size_t next_idx = 1;
	while (true) {
		curr = memmap_regions_at(&map->regions, curr_idx);
		next = memmap_regions_at(&map->regions, next_idx);

		// Can only merge two regions of the same type
		if (curr->type != next->type) {
			curr_idx += 1;
			next_idx += 1;
			if (next_idx >= map->regions.size) break;
			continue;
		}

		// Are the regions overlapping/adjacent
		if (curr->base + curr->size < next->base) {
			curr_idx += 1;
			next_idx += 1;
			if (next_idx >= map->regions.size) break;
			continue;
		}


		if (next->base + next->size > curr->base + curr->size) curr->size = next->base + next->size - curr->base;
		memmap_regions_remove(&map->regions, next_idx, NULL);
		if (next_idx >= map->regions.size) break;
	}

	return 0;
}

static int memmap_add_region(memmap_map_t* map, memmap_region_t region) {
	for (size_t i = 0; i <= map->regions.size; i++) {
		if (i == map->regions.size) {
			if (memmap_regions_push_back(&map->regions, region) == 1) return 1;
			break;
		}
		const memmap_region_t* prev = memmap_regions_const_at(&map->regions, i);
		if (prev == NULL) return 1;
		if (prev->base > region.base) {
			if (memmap_regions_insert(&map->regions, i, region) == 1) return 1;
			break;
		}
	}
	return 0;
}

int memmap_add_ram(memmap_map_t* map) {
	if (map == NULL || tree.data == NULL || tree.size != dtb_size) return 1;

	fdt_memory_iter_t mem_iter;
	fdt_result_t result = fdt_memory_iter_init(&tree, &mem_iter);
	if (result == FDT_NO_MORE_RANGES) return 0;
	if (result != FDT_SUCCESS) return 1;

	fdt_range_t range;
	memmap_region_t region;
	while ((result = fdt_memory_iter_next(&mem_iter, &range)) == FDT_SUCCESS) {
		if (UINT64_MAX - range.base < range.size) return 1;
		region = (memmap_region_t){range.base, range.size, MEM_USABLE};
		if (memmap_add_region(map, region) == 1) return 1;
	}
	if (result !=  FDT_NO_MORE_RANGES) return 1;
	if (map->regions.size == 0) return 1;

	if (memmap_merge(map) == 1) return 1;

	return 0;
}

static int memmap_reserve_region(memmap_map_t* map, memmap_region_t res_region) {
	uint64_t res_start = res_region.base;
	uint64_t res_size = res_region.size;
	uint64_t res_end = res_start + res_size;

	memmap_regions_dynamic_array new;
	if (memmap_regions_init(&new, &allocator) == 1) return 1;

	for (size_t i = 0; i < map->regions.size; i++) {
		memmap_region_t* mem_region = memmap_regions_at(&map->regions, i);
		if (mem_region->type != MEM_USABLE) continue;

		uint64_t ram_start = mem_region->base;
		uint64_t ram_size = mem_region->size;
		uint64_t ram_end = ram_start + ram_size;

		if (ram_end <= res_start || res_end <= ram_start) {
			if (memmap_regions_push_back(&new, *mem_region) == 1) {
				memmap_regions_destroy(&new);
				return 1;
			}
			continue;
		}

		if (ram_start < res_start) { // Add left piece
			if (memmap_regions_push_back(&new, (memmap_region_t){mem_region->base, res_start - ram_start, MEM_USABLE}) == 1) {
				memmap_regions_destroy(&new);
				return 1;
			}
		}
		if (res_end < ram_end) { // Add right piece
			if (memmap_regions_push_back(&new, (memmap_region_t){res_end, ram_end - res_end, MEM_USABLE}) == 1) {
				memmap_regions_destroy(&new);
				return 1;
			}
		}
	}

	memmap_regions_destroy(&map->regions);
	map->regions = new;

	return 0;
}

static int memmap_add_memreserve(memmap_map_t* map) {
	fdt_memreserve_iter_t res_iter;
	fdt_result_t result = fdt_memreserve_iter_init(&tree, &res_iter);
	if (result == FDT_NO_MORE_RANGES) return 0;
	if (result != FDT_SUCCESS) return 1;

	fdt_range_t range;
	memmap_region_t region;
	while ((result = fdt_memreserve_iter_next(&tree, &res_iter, &range)) == FDT_SUCCESS) {
		if (UINT64_MAX - range.base < range.size) return 1;
		region = (memmap_region_t){range.base, range.size, MEM_RESERVED};
		if (memmap_reserve_region(map, region) == 1) return 1;
	}
	if (result !=  FDT_NO_MORE_RANGES) return 1;
	if (memmap_merge(map) == 1) return 1;

	return 0;
}

static int memmap_add_reserved(memmap_map_t* map) {
	fdt_reserved_iter_t res_iter;
	fdt_result_t result = fdt_reserved_iter_init(&tree, &res_iter);
	if (result == FDT_NO_MORE_RANGES) return 0;
	if (result != FDT_SUCCESS) return 1;

	fdt_range_t range;
	memmap_region_t region;
	while ((result = fdt_reserved_iter_next(&res_iter, &range)) == FDT_SUCCESS) {
		if (UINT64_MAX - range.base < range.size) return 1;
		region = (memmap_region_t){range.base, range.size, MEM_RESERVED};
		if (memmap_reserve_region(map, region) == 1) return 1;
	}
	if (result !=  FDT_NO_MORE_RANGES) return 1;
	if (memmap_merge(map) == 1) return 1;

	return 0;
}

int memmap_reserve(memmap_map_t* map) {
	if (map == NULL || tree.data == NULL || tree.size != dtb_size) return 1;

	if (memmap_add_memreserve(map) == 1) return 1;
	if (memmap_add_reserved(map) == 1) return 1;

	return 0;
}

int memmap_finalize(memmap_map_t* map) {
	if (map == NULL) return 1;

	uint64_t new_base, new_end;
	memmap_regions_dynamic_array new;
	if (memmap_regions_init(&new, &allocator) == 1) return 1;
	for (size_t i = 0; i < map->regions.size; i++) {
		memmap_region_t* region = memmap_regions_at(&map->regions, i);

		if ((new_base = u64_align_up(region->base, PAGE_SIZE)) < region->base) continue;
		new_end = u64_align_down(region->base + region->size, PAGE_SIZE);

		if (new_base >= new_end) continue;
		if (memmap_regions_push_back(&new, (memmap_region_t){new_base, new_end - new_base, MEM_USABLE}) == 1) {
			memmap_regions_destroy(&new);
			return 1;
		}
	}
	memmap_regions_destroy(&map->regions);
	map->regions = new;

	return 0;
}

int memmap_next_usable();
