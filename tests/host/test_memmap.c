#include "et.h"

#define FREE_LIST_IMPLEMENTATION
#include "alloc.h"
#include "memmap.h"
#include "fdt_builder.h"

freelist_allocator _allocator;
_Alignas(16) uint8_t _allocator_memory[ALLOCATOR_ARENA_SIZE];

static bool allocator_ready;
static bool memmap_ready;
static memmap_map_t map;

#define REQUIRE_ALLOCATOR() VERIFY(allocator_ready)
#define REQUIRE_MEMMAP() VERIFY(memmap_ready)

void setup(void) {
	map = (memmap_map_t){0};
	allocator_ready = freelist_init(&_allocator, _allocator_memory, sizeof(_allocator_memory)) == FREELIST_OK;
}

static bool init_dtb(const test_dtb_spec_t* spec) {
	return allocator_ready && test_dtb_build(spec) != 0;
}

static bool init_map(const test_dtb_spec_t* spec) {
	if (!init_dtb(spec)) return false;
	return memmap_init(&map, buffer) == 0;
}

void teardown(){}

TEST_GROUP("test memmap") {
	TEST("initialization") {
		const test_dtb_spec_t spec = {0};
		init_dtb(&spec);
		memmap_map_t fresh = {0};
		VERIFY(memmap_init(&fresh, buffer) == 0);
		VERIFY(memmap_init(NULL, buffer) == 1);
		VERIFY(memmap_init(&fresh, NULL) == 1);
	}

	TEST("add two separate RAM ranges") {
		const test_range_t ram[] = {
			{0x1000, 0x2000},
			{0x8000, 0x4000},
		};
		const test_dtb_spec_t spec = {
			.ram = ram,
			.ram_count = sizeof(ram) / sizeof(ram[0]),
		};
		VERIFY(init_map(&spec));

		VERIFY(memmap_add_ram(&map) == 0);
		VERIFY(map.regions.size == 2);
		const memmap_region_t* first = memmap_regions_const_at(&map.regions, 0);
		const memmap_region_t* second = memmap_regions_const_at(&map.regions, 1);
		VERIFY(first->base == 0x1000 && first->size == 0x2000);
		VERIFY(first->type == MEM_USABLE);
		VERIFY(second->base == 0x8000 && second->size == 0x4000);
		VERIFY(second->type == MEM_USABLE);
	}

	TEST("add two adjacent RAM ranges") {
		const test_range_t ram[] = {
			{0x1000, 0x3000},
			{0x4000, 0x4000},
		};
		const test_dtb_spec_t spec = {
			.ram = ram,
			.ram_count = sizeof(ram) / sizeof(ram[0]),
		};
		VERIFY(init_map(&spec));

		VERIFY(memmap_add_ram(&map) == 0);
		VERIFY(map.regions.size == 1);
		const memmap_region_t* first = memmap_regions_const_at(&map.regions, 0);
		VERIFY(first->base == 0x1000 && first->size == 0x7000);
		VERIFY(first->type == MEM_USABLE);
	}

	TEST("add two overlapping RAM ranges") {
		const test_range_t ram[] = {
			{0x1000, 0x3000},
			{0x3000, 0x4000},
		};
		const test_dtb_spec_t spec = {
			.ram = ram,
			.ram_count = sizeof(ram) / sizeof(ram[0]),
		};
		VERIFY(init_map(&spec));

		VERIFY(memmap_add_ram(&map) == 0);
		VERIFY(map.regions.size == 1);
		const memmap_region_t* first = memmap_regions_const_at(&map.regions, 0);
		VERIFY(first->base == 0x1000 && first->size == 0x6000);
		VERIFY(first->type == MEM_USABLE);
	}

	TEST("add encompassing RAM ranges") {
		const test_range_t ram[] = {
			{0x1000, 0x5000},
			{0x3000, 0x1000},
		};
		const test_dtb_spec_t spec = {
			.ram = ram,
			.ram_count = sizeof(ram) / sizeof(ram[0]),
		};
		VERIFY(init_map(&spec));

		VERIFY(memmap_add_ram(&map) == 0);
		VERIFY(map.regions.size == 1);
		const memmap_region_t* first = memmap_regions_const_at(&map.regions, 0);
		VERIFY(first->base == 0x1000 && first->size == 0x5000);
		VERIFY(first->type == MEM_USABLE);
	}

	TEST("add map separate unordered RAM ranges") {
		const test_range_t ram[] = {
			{0x5000, 0x2000},
			{0x1000, 0x2000},
		};
		const test_dtb_spec_t spec = {
			.ram = ram,
			.ram_count = sizeof(ram) / sizeof(ram[0]),
		};
		VERIFY(init_map(&spec));

		VERIFY(memmap_add_ram(&map) == 0);
		VERIFY(map.regions.size == 2);
		const memmap_region_t* first = memmap_regions_const_at(&map.regions, 0);
		const memmap_region_t* second = memmap_regions_const_at(&map.regions, 1);
		VERIFY(first->base == 0x1000 && first->size == 0x2000);
		VERIFY(first->type == MEM_USABLE);
		VERIFY(second->base == 0x5000 && second->size == 0x2000);
		VERIFY(second->type == MEM_USABLE);
	}

	TEST("add map adjacent/overlapping unordered RAM ranges") {
		const test_range_t ram[] = {
			{0x5000, 0x2000},
			{0x3000, 0x3000},
			{0x10000, 0x4000},
			{0x8000, 0x9000},
		};
		const test_dtb_spec_t spec = {
			.ram = ram,
			.ram_count = sizeof(ram) / sizeof(ram[0]),
		};
		VERIFY(init_map(&spec));

		VERIFY(memmap_add_ram(&map) == 0);
		VERIFY(map.regions.size == 2);
		const memmap_region_t* first = memmap_regions_const_at(&map.regions, 0);
		const memmap_region_t* second = memmap_regions_const_at(&map.regions, 1);
		VERIFY(first->base == 0x3000 && first->size == 0x4000);
		VERIFY(first->type == MEM_USABLE);
		VERIFY(second->base == 0x8000 && second->size == 0xc000);
		VERIFY(second->type == MEM_USABLE);
	}

	TEST("add map separate RAM/reserved ranges") {
		const test_range_t ram[] = {
			{0x1000, 0x2000},
			{0x8000, 0x4000},
		};
		const test_range_t res[] = {
			{0x4000, 0x1000},
			{0x10000, 0x2000},
		};
		const test_dtb_spec_t spec = {
			.ram = ram,
			.ram_count = sizeof(ram) / sizeof(ram[0]),
			.reservations = res,
			.reservation_count = sizeof(res) / sizeof(res[0])
		};
		VERIFY(init_map(&spec));

		VERIFY(memmap_add_ram(&map) == 0);
		VERIFY(memmap_reserve(&map) == 0);
		VERIFY(map.regions.size == 2);
		const memmap_region_t* first = memmap_regions_const_at(&map.regions, 0);
		const memmap_region_t* second = memmap_regions_const_at(&map.regions, 1);
		VERIFY(first->base == 0x1000 && first->size == 0x2000);
		VERIFY(first->type == MEM_USABLE);
		VERIFY(second->base == 0x8000 && second->size == 0x4000);
		VERIFY(second->type == MEM_USABLE);
	}

	TEST("add map overlapping RAM/reserved ranges") {
		const test_range_t ram[] = {
			{0x2000, 0x5000},
		};
		const test_range_t res[] = {
			{0x4000, 0x4000},
		};
		const test_dtb_spec_t spec = {
			.ram = ram,
			.ram_count = sizeof(ram) / sizeof(ram[0]),
			.reservations = res,
			.reservation_count = sizeof(res) / sizeof(res[0])
		};
		VERIFY(init_map(&spec));

		VERIFY(memmap_add_ram(&map) == 0);
		VERIFY(memmap_reserve(&map) == 0);
		VERIFY(map.regions.size == 1);
		const memmap_region_t* first = memmap_regions_const_at(&map.regions, 0);
		VERIFY(first->base == 0x2000 && first->size == 0x2000);
		VERIFY(first->type == MEM_USABLE);
	}

	TEST("add map overlapping RAM/reserved ranges") {
		const test_range_t ram[] = {
			{0x6000, 0x1000},
		};
		const test_range_t res[] = {
			{0x4000, 0x4000},
		};
		const test_dtb_spec_t spec = {
			.ram = ram,
			.ram_count = sizeof(ram) / sizeof(ram[0]),
			.reservations = res,
			.reservation_count = sizeof(res) / sizeof(res[0])
		};
		VERIFY(init_map(&spec));

		VERIFY(memmap_add_ram(&map) == 0);
		VERIFY(memmap_reserve(&map) == 0);
		VERIFY(map.regions.size == 0);
	}

	TEST("add map overlapping RAM/reserved ranges") {
		const test_range_t ram[] = {
			{0x5000, 0x2000},
		};
		const test_range_t res[] = {
			{0x4000, 0x2000},
		};
		const test_dtb_spec_t spec = {
			.ram = ram,
			.ram_count = sizeof(ram) / sizeof(ram[0]),
			.reservations = res,
			.reservation_count = sizeof(res) / sizeof(res[0])
		};
		VERIFY(init_map(&spec));

		VERIFY(memmap_add_ram(&map) == 0);
		VERIFY(memmap_reserve(&map) == 0);
		VERIFY(map.regions.size == 1);
		const memmap_region_t* first= memmap_regions_const_at(&map.regions, 0);
		VERIFY(first->base == 0x6000 && first->size == 0x1000);
		VERIFY(first->type == MEM_USABLE);
	}

	TEST("add map overlapping RAM/reserved ranges") {
		const test_range_t ram[] = {
			{0x2000, 0x7000},
		};
		const test_range_t res[] = {
			{0x4000, 0x1000},
		};
		const test_dtb_spec_t spec = {
			.ram = ram,
			.ram_count = sizeof(ram) / sizeof(ram[0]),
			.reservations = res,
			.reservation_count = sizeof(res) / sizeof(res[0])
		};
		VERIFY(init_map(&spec));

		VERIFY(memmap_add_ram(&map) == 0);
		VERIFY(memmap_reserve(&map) == 0);
		VERIFY(map.regions.size == 2);
		const memmap_region_t* first = memmap_regions_const_at(&map.regions, 0);
		const memmap_region_t* second = memmap_regions_const_at(&map.regions, 1);

		VERIFY(first->base == 0x2000 && first->size == 0x2000);
		VERIFY(first->type == MEM_USABLE);
		VERIFY(second->base == 0x5000 && second->size == 0x4000);
		VERIFY(second->type == MEM_USABLE);
	}
	TEST("reservations touching RAM endpoints do not overlap") {
		const test_range_t ram[] = {{0x2000, 0x3000}};
		const test_range_t res[] = {{0x1000, 0x1000}, {0x5000, 0x1000}};
		const test_dtb_spec_t spec = {
			.ram = ram, .ram_count = 1,
			.reservations = res, .reservation_count = 2,
		};
		VERIFY(init_map(&spec));
		VERIFY(memmap_add_ram(&map) == 0);
		VERIFY(memmap_reserve(&map) == 0);
		VERIFY(map.regions.size == 1);
		VERIFY(map.regions.data[0].base == 0x2000);
		VERIFY(map.regions.data[0].size == 0x3000);
		VERIFY(map.regions.data[0].type == MEM_USABLE);
	}

	TEST("reservation spans RAM regions and holes") {
		const test_range_t ram[] = {
			{0x1000, 0x2000}, {0x4000, 0x1000}, {0x7000, 0x3000},
		};
		const test_range_t res[] = {{0x2000, 0x6000}};
		const test_dtb_spec_t spec = {
			.ram = ram, .ram_count = 3,
			.reservations = res, .reservation_count = 1,
		};
		VERIFY(init_map(&spec));
		VERIFY(memmap_add_ram(&map) == 0);
		VERIFY(memmap_reserve(&map) == 0);
		VERIFY(map.regions.size == 2);
		VERIFY(map.regions.data[0].base == 0x1000);
		VERIFY(map.regions.data[0].size == 0x1000);
		VERIFY(map.regions.data[1].base == 0x8000);
		VERIFY(map.regions.data[1].size == 0x2000);
		VERIFY(map.regions.data[0].type == MEM_USABLE);
		VERIFY(map.regions.data[1].type == MEM_USABLE);
	}

	TEST("unordered overlapping reservations from both sources are idempotent") {
		const test_range_t ram[] = {{0x1000, 0x9000}};
		const test_range_t res[] = {{0x6000, 0x2000}, {0x3000, 0x2000}};
		const test_range_t reserved[] = {{0x4000, 0x3000}, {0x3000, 0x2000}};
		const test_dtb_spec_t spec = {
			.ram = ram, .ram_count = 1,
			.reservations = res, .reservation_count = 2,
			.reserved_memory = reserved, .reserved_memory_count = 2,
		};
		VERIFY(init_map(&spec));
		VERIFY(memmap_add_ram(&map) == 0);
		for (size_t pass = 0; pass < 2; pass++) {
			VERIFY(memmap_reserve(&map) == 0);
			VERIFY(map.regions.size == 2);
			VERIFY(map.regions.data[0].base == 0x1000);
			VERIFY(map.regions.data[0].size == 0x2000);
			VERIFY(map.regions.data[1].base == 0x8000);
			VERIFY(map.regions.data[1].size == 0x2000);
			VERIFY(map.regions.data[0].type == MEM_USABLE);
			VERIFY(map.regions.data[1].type == MEM_USABLE);
		}
	}

	TEST("exact reservation leaves an empty map on repeated calls") {
		const test_range_t ram[] = {{0x1000, 0x3000}};
		const test_dtb_spec_t spec = {
			.ram = ram, .ram_count = 1,
			.reserved_memory = ram, .reserved_memory_count = 1,
		};
		VERIFY(init_map(&spec));
		VERIFY(memmap_add_ram(&map) == 0);
		VERIFY(memmap_reserve(&map) == 0);
		VERIFY(map.regions.size == 0);
		VERIFY(memmap_reserve(&map) == 0);
		VERIFY(map.regions.size == 0);
	}

	TEST("subtract reservation above 4 GiB without truncation") {
		const test_range_t ram[] = {{UINT64_C(0x100001000), 0x6000}};
		const test_range_t res[] = {{UINT64_C(0x100003000), 0x1000}};
		const test_dtb_spec_t spec = {
			.ram = ram, .ram_count = 1,
			.reservations = res, .reservation_count = 1,
		};
		VERIFY(init_map(&spec));
		VERIFY(memmap_add_ram(&map) == 0);
		VERIFY(memmap_reserve(&map) == 0);
		VERIFY(map.regions.size == 2);
		VERIFY(map.regions.data[0].base == UINT64_C(0x100001000));
		VERIFY(map.regions.data[0].size == 0x2000);
		VERIFY(map.regions.data[1].base == UINT64_C(0x100004000));
		VERIFY(map.regions.data[1].size == 0x3000);
		VERIFY(map.regions.data[0].type == MEM_USABLE);
		VERIFY(map.regions.data[1].type == MEM_USABLE);
	}

	/* Policy: an endpoint that cannot be represented must report an error. */
	TEST("reject overflowing reservation endpoint from either source") {
		const test_range_t ram[] = {{0x1000, 0x4000}};
		const test_range_t res[] = {{UINT64_MAX - 0x7ff, 0x1000}};
		for (size_t source = 0; source < 2; source++) {
			const test_dtb_spec_t spec = {
				.ram = ram, .ram_count = 1,
				.reservations = res, .reservation_count = source == 0 ? 1 : 0,
				.reserved_memory = res, .reserved_memory_count = source == 1 ? 1 : 0,
			};
			VERIFY(init_map(&spec));
			VERIFY(memmap_add_ram(&map) == 0);
			int result = memmap_reserve(&map);
			memmap_regions_destroy(&map.regions);
			VERIFY(result == 1);
		}
	}

	TEST("reject overflowing RAM endpoint") {
		const test_range_t ram[] = {{UINT64_MAX - 0x7ff, 0x1000}};
		const test_dtb_spec_t spec = {.ram = ram, .ram_count = 1};
		VERIFY(init_map(&spec));
		VERIFY(memmap_add_ram(&map) == 1);
	}
	TEST("finalize preserves aligned ranges and is idempotent") {
		const test_range_t ram[] = {{0x1000, 0x2000}, {UINT64_C(0x100008000), 0x3000}};
		const test_dtb_spec_t spec = {.ram = ram, .ram_count = 2};
		VERIFY(init_map(&spec));
		VERIFY(memmap_add_ram(&map) == 0);
		for (size_t pass = 0; pass < 2; pass++) {
			VERIFY(memmap_finalize(&map) == 0);
			VERIFY(map.regions.size == 2);
			for (size_t i = 0; i < 2; i++) {
				VERIFY(map.regions.data[i].base == ram[i].base);
				VERIFY(map.regions.data[i].size == ram[i].size);
				VERIFY(map.regions.data[i].type == MEM_USABLE);
			}
		}
	}

	TEST("finalize aligns original endpoints inward rather than aligning size") {
		/* [0x1001, 0x4001) contains only [0x2000, 0x4000) as whole pages. */
		const test_range_t ram[] = {{0x1001, 0x3000}, {0x8000, 0x2001}};
		const test_dtb_spec_t spec = {.ram = ram, .ram_count = 2};
		VERIFY(init_map(&spec));
		VERIFY(memmap_add_ram(&map) == 0);
		VERIFY(memmap_finalize(&map) == 0);
		VERIFY(map.regions.size == 2);
		VERIFY(map.regions.data[0].base == 0x2000);
		VERIFY(map.regions.data[0].size == 0x2000);
		VERIFY(map.regions.data[1].base == 0x8000);
		VERIFY(map.regions.data[1].size == 0x2000);
	}

	TEST("finalize removes consecutive small ranges and preserves survivor order") {
		const test_range_t ram[] = {
			{0x1001, 0xfff}, {0x3000, 0x800},
			{0x5001, 0x2fff}, {0xa001, 0x1000},
			{0xd000, 0x1000}, {0xf001, 0x100},
		};
		const test_dtb_spec_t spec = {.ram = ram, .ram_count = 6};
		VERIFY(init_map(&spec));
		VERIFY(memmap_add_ram(&map) == 0);
		VERIFY(memmap_finalize(&map) == 0);
		VERIFY(map.regions.size == 2);
		VERIFY(map.regions.data[0].base == 0x6000);
		VERIFY(map.regions.data[0].size == 0x2000);
		VERIFY(map.regions.data[1].base == 0xd000);
		VERIFY(map.regions.data[1].size == 0x1000);
		VERIFY(map.regions.data[0].type == MEM_USABLE);
		VERIFY(map.regions.data[1].type == MEM_USABLE);
	}

	TEST("finalize succeeds with no complete pages and on an empty map") {
		const test_range_t ram[] = {{0x1001, 0x1000}};
		const test_dtb_spec_t spec = {.ram = ram, .ram_count = 1};
		VERIFY(init_map(&spec));
		VERIFY(memmap_finalize(&map) == 0);
		VERIFY(map.regions.size == 0);
		VERIFY(memmap_add_ram(&map) == 0);
		VERIFY(memmap_finalize(&map) == 0);
		VERIFY(map.regions.size == 0);
		VERIFY(memmap_finalize(&map) == 0);
		VERIFY(map.regions.size == 0);
	}

	TEST("merge adjacent subpage RAM before finalizing") {
		const test_range_t ram[] = {{0x1800, 0x800}, {0x1000, 0x800}};
		const test_dtb_spec_t spec = {.ram = ram, .ram_count = 2};
		VERIFY(init_map(&spec));
		VERIFY(memmap_add_ram(&map) == 0);
		VERIFY(memmap_finalize(&map) == 0);
		VERIFY(map.regions.size == 1);
		VERIFY(map.regions.data[0].base == 0x1000);
		VERIFY(map.regions.data[0].size == 0x1000);
	}

	TEST("finalize excludes every page touched by byte-sized reservations") {
		const test_range_t ram[] = {{0x1000, 0x7000}};
		const test_range_t res[] = {{0x3000, 1}};
		const test_range_t reserved[] = {{0x5fff, 2}};
		const test_dtb_spec_t spec = {
			.ram = ram, .ram_count = 1,
			.reservations = res, .reservation_count = 1,
			.reserved_memory = reserved, .reserved_memory_count = 1,
		};
		VERIFY(init_map(&spec));
		VERIFY(memmap_add_ram(&map) == 0);
		VERIFY(memmap_reserve(&map) == 0);
		VERIFY(memmap_finalize(&map) == 0);
		VERIFY(map.regions.size == 3);
		VERIFY(map.regions.data[0].base == 0x1000);
		VERIFY(map.regions.data[0].size == 0x2000);
		VERIFY(map.regions.data[1].base == 0x4000);
		VERIFY(map.regions.data[1].size == 0x1000);
		VERIFY(map.regions.data[2].base == 0x7000);
		VERIFY(map.regions.data[2].size == 0x1000);
		for (size_t i = 0; i < 3; i++) VERIFY(map.regions.data[i].type == MEM_USABLE);
	}

	TEST("finalize drops a valid top-of-address-space fragment without wrapping") {
		/* The endpoint is representable, but rounding the base up is not.
		 * This valid range contains no complete page, so it should disappear. */
		const test_range_t ram[] = {{UINT64_MAX - 0x7ff, 0x7ff}};
		const test_dtb_spec_t spec = {.ram = ram, .ram_count = 1};
		VERIFY(init_map(&spec));
		VERIFY(memmap_add_ram(&map) == 0);
		VERIFY(memmap_finalize(&map) == 0);
		VERIFY(map.regions.size == 0);
	}

	TEST("finalize rejects a null map") {
		VERIFY(memmap_finalize(NULL) == 1);
	}
/*
		for (size_t i = 0; i < map.regions.size; i++) {
			printf("\n[TYPE: %s ; BASE: 0x%llx; SIZE:0x%llx]", map.regions.data[i].type == MEM_USABLE ? "USABLE" : "RESERVED",
			map.regions.data[i].base, map.regions.data[i].size);
		}
		printf("\n");
*/
}
