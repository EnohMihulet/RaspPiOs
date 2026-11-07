#ifndef TESTS_HOST_FDT_BUILDER_H
#define TESTS_HOST_FDT_BUILDER_H

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

/* Include fdt.h first; the suite supplies its implementation. */
/* Each suite owns a private mutable buffer. dtb_begin() resets it. */
/* Wire tokens are private to fdt.h's implementation. */
#ifndef FDT_IMPLEMENTATION
enum {
	FDT_BEGIN_NODE = 0x01,
	FDT_END_NODE = 0x02,
	FDT_PROP = 0x03,
	FDT_NOP = 0x04,
	FDT_END = 0x09,
};
#endif


static uint8_t buffer[2048];
static size_t cursor;
static size_t structure_offset;
static bool builder_failed;

typedef struct {
	uint64_t base, size;
} test_range_t;

typedef struct {
	const test_range_t* ram;
	size_t ram_count;
	const test_range_t* reservations;
	size_t reservation_count;
	const test_range_t* reserved_memory;
	size_t reserved_memory_count;
} test_dtb_spec_t;

static inline bool builder_room(size_t offset, size_t length) {
	if (builder_failed || offset > sizeof(buffer) || length > sizeof(buffer) - offset) {
		builder_failed = true;
		return false;
	}
	return true;
}

static void write_be32(size_t offset, uint32_t value) {
	if (!builder_room(offset, 4)) return;
	buffer[offset] = (uint8_t)(value >> 24);
	buffer[offset + 1] = (uint8_t)(value >> 16);
	buffer[offset + 2] = (uint8_t)(value >> 8);
	buffer[offset + 3] = (uint8_t)value;
}

static void emit_word(uint32_t value) {
	if (!builder_room(cursor, 4)) return;
	write_be32(cursor, value);
	cursor += 4;
}

static inline void emit_data(const void* data, size_t length) {
	if (!builder_room(cursor, length)) return;
	size_t padded = (length + 3) & ~(size_t)3;
	if (!builder_room(cursor, padded)) return;
	if (length != 0) memcpy(buffer + cursor, data, length);
	cursor += padded;
}

static fdt_node_t begin_node(const char* name) {
	fdt_node_t node = {cursor};
	emit_word(FDT_BEGIN_NODE);
	size_t length = strlen(name) + 1;
	emit_data(name, length);
	return node;
}

enum { ADDRESS_CELLS = 0, SIZE_CELLS = 15, DEVICE_TYPE = 27, REG = 39, STATUS = 43, RANGES = 50 };

static const char strings[] = "#address-cells\0#size-cells\0device_type\0reg\0status\0ranges";

static void property(uint32_t name, const void* data, uint32_t length) {
	emit_word(FDT_PROP);
	emit_word(length);
	emit_word(name);
	emit_data(data, length);
}

static void cells(void) {
	const uint8_t two[] = {0, 0, 0, 2};
	property(ADDRESS_CELLS, two, sizeof(two));
	property(SIZE_CELLS, two, sizeof(two));
}

static void emit_u64(uint64_t value) {
	emit_word((uint32_t)(value >> 32));
	emit_word((uint32_t)value);
}

static void reg_range(uint64_t base, uint64_t size) {
	emit_word(FDT_PROP);
	emit_word(16);
	emit_word(REG);
	emit_u64(base);
	emit_u64(size);
}

/* Leaves the structure block ready for begin_node(""). */
static void dtb_begin(const test_range_t* reservations, size_t count) {
	memset(buffer, 0, sizeof(buffer));
	builder_failed = false;
	cursor = 40;
	if (count != 0 && reservations == NULL) builder_failed = true;
	for (size_t i = 0; i < count && !builder_failed; i++) {
		emit_u64(reservations[i].base);
		emit_u64(reservations[i].size);
	}
	emit_u64(0);
	emit_u64(0);
	structure_offset = cursor;
}

/* Call after closing the root. Returns zero if the buffer was too small. */
static size_t dtb_finish(void) {
	emit_word(FDT_END);
	size_t strings_offset = cursor;
	emit_data(strings, sizeof(strings));
	write_be32(FDT_MAGIC_OFFSET, FDT_MAGIC);
	write_be32(FDT_TOTALSIZE_OFFSET, (uint32_t)cursor);
	write_be32(FDT_OFF_DT_STRUCT_OFFSET, (uint32_t)structure_offset);
	write_be32(FDT_OFF_DT_STRING_OFFSET, (uint32_t)strings_offset);
	write_be32(FDT_OFF_MEM_RSVMAP_OFFSET, 40);
	write_be32(FDT_VERSION_OFFSET, FDT_VERSION);
	write_be32(FDT_LAST_CMP_VERSION_OFFSET, 16);
	write_be32(FDT_SIZE_DT_STRING_OFFSET, sizeof(strings));
	write_be32(FDT_SIZE_DT_STRUCT_OFFSET, (uint32_t)(strings_offset - structure_offset));
	return builder_failed ? 0 : cursor;
}

/* Encodes ranges in input order, without sorting or merging them.
 * The returned size describes buffer; zero means invalid input or no space. */
static inline size_t test_dtb_build(const test_dtb_spec_t* spec) {
	if (spec == NULL) return 0;
	dtb_begin(spec->reservations, spec->reservation_count);
	if ((spec->ram_count && !spec->ram) ||
		(spec->reserved_memory_count && !spec->reserved_memory)) return 0;
	begin_node("");
	cells();
	for (size_t i = 0; i < spec->ram_count && !builder_failed; i++) {
		char name[40];
		snprintf(name, sizeof(name), "memory@%llx", (unsigned long long)spec->ram[i].base);
		begin_node(name);
		property(DEVICE_TYPE, "memory", sizeof("memory"));
		reg_range(spec->ram[i].base, spec->ram[i].size);
		emit_word(FDT_END_NODE);
	}
	if (spec->reserved_memory_count) {
		begin_node("reserved-memory");
		cells();
		property(RANGES, NULL, 0);
		for (size_t i = 0; i < spec->reserved_memory_count && !builder_failed; i++) {
			char name[40];
			snprintf(name, sizeof(name), "reserved@%llx", (unsigned long long)spec->reserved_memory[i].base);
			begin_node(name);
			reg_range(spec->reserved_memory[i].base, spec->reserved_memory[i].size);
			emit_word(FDT_END_NODE);
		}
		emit_word(FDT_END_NODE);
	}
	emit_word(FDT_END_NODE);
	return dtb_finish();
}

#endif
