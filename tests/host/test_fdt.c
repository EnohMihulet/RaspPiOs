#include "et.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define FDT_IMPLEMENTATION
#include "../../lib/fdt.h"
#include "fdt_builder.h"

static fdt_tree_t tree;
static fdt_result_t setup_result;
static fdt_node_t root, memory, reserved, leaf;

static void fixture_setup(void) {
	const test_range_t reservations[] = {{UINT64_C(0x123456000), 0x2000}};
	dtb_begin(reservations, 1);
	tree = (fdt_tree_t){0};
	root = begin_node("");
	cells();
	emit_word(FDT_NOP);
	begin_node("bus");
	leaf = begin_node("leaf");
	property(STATUS, "okay", sizeof("okay"));
	emit_word(FDT_END_NODE);
	emit_word(FDT_END_NODE);
	memory = begin_node("memory@1000");
	property(DEVICE_TYPE, "memory", sizeof("memory"));
	reg_range(0x1000, 0x2000);
	emit_word(FDT_END_NODE);
	begin_node("other-device");
	property(DEVICE_TYPE, "serial", sizeof("serial"));
	emit_word(FDT_END_NODE);
	begin_node("memory@8000");
	property(DEVICE_TYPE, "memory", sizeof("memory"));
	reg_range(0x8000, 0x4000);
	emit_word(FDT_END_NODE);
	reserved = begin_node("reserved-memory");
	cells();
	property(RANGES, NULL, 0);
	begin_node("disabled-first");
	property(STATUS, "disabled", sizeof("disabled"));
	reg_range(0xdead, 0x100);
	emit_word(FDT_END_NODE);
	begin_node("dynamic");
	emit_word(FDT_END_NODE);
	begin_node("reserved@9000");
	reg_range(0x9000, 0x100);
	emit_word(FDT_END_NODE);
	begin_node("disabled-middle");
	property(STATUS, "disabled", sizeof("disabled"));
	reg_range(0xbeef, 0x100);
	emit_word(FDT_END_NODE);
	begin_node("dynamic-middle");
	emit_word(FDT_END_NODE);
	begin_node("reserved@a000");
	property(STATUS, "okay", sizeof("okay"));
	reg_range(0xa000, 0x200);
	emit_word(FDT_END_NODE);
	emit_word(FDT_END_NODE);
	emit_word(FDT_END_NODE);
	size_t size = dtb_finish();
	setup_result = fdt_init(&tree, buffer, size);
}

#define REQUIRE_TREE() VERIFY(setup_result == FDT_SUCCESS)

void setup(void) {
	fixture_setup();
}

void teardown(void) {}

TEST_GROUP("test fdt") {
	TEST("initialization") {
		fdt_tree_t fresh = {0};
		VERIFY(fdt_init(&fresh, buffer, cursor) == FDT_SUCCESS);
		VERIFY(fresh.data == buffer);
		VERIFY(fresh.size == cursor);
	}

	TEST("init invalid arguments") {
		VERIFY(fdt_init(NULL, buffer, cursor) == FDT_ERROR);
		VERIFY(fdt_init(&tree, NULL, cursor) == FDT_ERROR);
		VERIFY(fdt_init(&tree, buffer, 0) == FDT_ERROR);
		const uint8_t short_header[4] = {0xd0, 0x0d, 0xfe, 0xed};
		VERIFY(fdt_init(&tree, short_header, sizeof(short_header)) == FDT_ERROR);
	}

	TEST("init invalid header") {
		write_be32(FDT_MAGIC_OFFSET, 0);
		VERIFY(fdt_init(&tree, buffer, cursor) == FDT_ERROR);
		write_be32(FDT_MAGIC_OFFSET, FDT_MAGIC);
		write_be32(FDT_VERSION_OFFSET, FDT_VERSION - 1);
		VERIFY(fdt_init(&tree, buffer, cursor) == FDT_ERROR);
		write_be32(FDT_VERSION_OFFSET, FDT_VERSION);
		write_be32(FDT_TOTALSIZE_OFFSET, (uint32_t)cursor + 1);
		VERIFY(fdt_init(&tree, buffer, cursor) == FDT_ERROR);
	}

	TEST("find ") {
		REQUIRE_TREE();
		fdt_node_t node;
		VERIFY(fdt_find_node(&tree, "", &node) == FDT_SUCCESS);
		VERIFY(node.offset == root.offset);
		VERIFY(fdt_find_node(&tree, "leaf", &node) == FDT_SUCCESS);
		VERIFY(node.offset == leaf.offset);
		VERIFY(fdt_find_node(&tree, "missing", &node) == FDT_NOT_FOUND);
		VERIFY(fdt_find_node(&tree, "memory", &node) == FDT_NOT_FOUND);
	}

	TEST("properties") {
		REQUIRE_TREE();
		fdt_property_t prop;
		const uint8_t expected[] = {0, 0, 0, 2};
		VERIFY(fdt_get_property(&tree, &root, "#size-cells", &prop) == FDT_SUCCESS);
		VERIFY(prop.length == sizeof(expected));
		VERIFY(memcmp(prop.data, expected, sizeof(expected)) == 0);
		VERIFY(fdt_get_property(&tree, &leaf, "status", &prop) == FDT_SUCCESS);
		VERIFY(prop.length == sizeof("okay"));
		VERIFY(memcmp(prop.data, "okay", sizeof("okay")) == 0);
		VERIFY(fdt_get_property(&tree, &leaf, "missing", &prop) == FDT_NOT_FOUND);
		/* A child's property must not be returned for its parent. */
		VERIFY(fdt_get_property(&tree, &root, "status", &prop) == FDT_NOT_FOUND);
	}

	TEST("traversal") {
		REQUIRE_TREE();
		fdt_node_t bus, child, sibling;
		VERIFY(fdt_find_node(&tree, "bus", &bus) == FDT_SUCCESS);
		VERIFY(fdt_get_child_node(&tree, &root, &child) == FDT_SUCCESS);
		VERIFY(child.offset == bus.offset);
		VERIFY(fdt_get_child_node(&tree, &bus, &child) == FDT_SUCCESS);
		VERIFY(child.offset == leaf.offset);
		VERIFY(fdt_get_child_node(&tree, &leaf, &child) == FDT_NO_MORE_NODES);
		VERIFY(fdt_get_sibling_node(&tree, &bus, &sibling) == FDT_SUCCESS);
		VERIFY(sibling.offset == memory.offset);
		VERIFY(fdt_get_sibling_node(&tree, &leaf, &sibling) == FDT_NO_MORE_NODES);
		VERIFY(fdt_get_sibling_node(&tree, &reserved, &sibling) == FDT_NO_MORE_NODES);
		VERIFY(fdt_get_sibling_node(&tree, &root, &sibling) == FDT_NO_MORE_NODES);
	}

	TEST("node invalid arguments") {
		REQUIRE_TREE();
		fdt_node_t node, invalid = {1};
		fdt_property_t prop;
		fdt_tree_t empty = {0};
		VERIFY(fdt_find_node(NULL, "", &node) == FDT_ERROR);
		VERIFY(fdt_find_node(&empty, "", &node) == FDT_ERROR);
		VERIFY(fdt_find_node(&tree, NULL, &node) == FDT_ERROR);
		VERIFY(fdt_find_node(&tree, "", NULL) == FDT_ERROR);
		VERIFY(fdt_get_property(NULL, &root, "status", &prop) == FDT_ERROR);
		VERIFY(fdt_get_property(&tree, NULL, "status", &prop) == FDT_ERROR);
		VERIFY(fdt_get_property(&tree, &root, NULL, &prop) == FDT_ERROR);
		VERIFY(fdt_get_property(&tree, &root, "status", NULL) == FDT_ERROR);
		VERIFY(fdt_get_property(&tree, &invalid, "status", &prop) == FDT_ERROR);
		VERIFY(fdt_get_child_node(NULL, &root, &node) == FDT_ERROR);
		VERIFY(fdt_get_child_node(&tree, NULL, &node) == FDT_ERROR);
		VERIFY(fdt_get_child_node(&tree, &root, NULL) == FDT_ERROR);
		VERIFY(fdt_get_child_node(&tree, &invalid, &node) == FDT_ERROR);
		VERIFY(fdt_get_sibling_node(NULL, &root, &node) == FDT_ERROR);
		VERIFY(fdt_get_sibling_node(&tree, NULL, &node) == FDT_ERROR);
		VERIFY(fdt_get_sibling_node(&tree, &root, NULL) == FDT_ERROR);
		VERIFY(fdt_get_sibling_node(&tree, &invalid, &node) == FDT_ERROR);
	}

	TEST("invalid token") {
		REQUIRE_TREE();
		fdt_node_t node;
		fdt_property_t prop;
		write_be32(root.offset, 0xff);
		VERIFY(fdt_find_node(&tree, "missing", &node) == FDT_ERROR);
		VERIFY(fdt_get_property(&tree, &root, "status", &prop) == FDT_ERROR);
		VERIFY(fdt_get_child_node(&tree, &root, &node) == FDT_ERROR);
		VERIFY(fdt_get_sibling_node(&tree, &root, &node) == FDT_ERROR);
	}

	TEST("token bounds") {
		REQUIRE_TREE();
		fdt_node_t node;
		fdt_property_t prop;
		fdt_parser_t parser;
		fdt_token_t token;
		size_t property_offset = root.offset + 8;

		/* A truncated property header must not read into the strings block. */
		write_be32(FDT_SIZE_DT_STRUCT_OFFSET, 12);
		VERIFY(fdt_get_property(&tree, &root, "#address-cells", &prop) == FDT_ERROR);
		fixture_setup();

		/* Oversized payloads must not wrap the cursor or escape the block. */
		write_be32(property_offset + 4, UINT32_MAX);
		VERIFY(fdt_internal_parser_init(&tree, &parser) == FDT_SUCCESS);
		parser.cursor = property_offset;
		VERIFY(fdt_internal_next_token(&tree, &parser, &token) == FDT_ERROR);
		VERIFY(parser.cursor == property_offset);
		fixture_setup();

		write_be32(property_offset + 8, sizeof(strings));
		VERIFY(fdt_get_child_node(&tree, &root, &node) == FDT_ERROR);
		fixture_setup();

		/* A string must terminate inside its own block. */
		size_t strings_offset = fdt_internal_read_be32(buffer + FDT_OFF_DT_STRING_OFFSET);
		memset(buffer + strings_offset, 'x', sizeof(strings));
		VERIFY(fdt_find_node(&tree, "missing", &node) == FDT_ERROR);
		fixture_setup();

		write_be32(FDT_SIZE_DT_STRUCT_OFFSET, 8);
		memset(buffer + root.offset + 4, 'x', 4);
		VERIFY(fdt_get_sibling_node(&tree, &root, &node) == FDT_ERROR);
		fixture_setup();

		node.offset = fdt_internal_read_be32(buffer + FDT_OFF_DT_STRUCT_OFFSET) + fdt_internal_read_be32(buffer + FDT_SIZE_DT_STRUCT_OFFSET);
		VERIFY(fdt_get_property(&tree, &node, "status", &prop) == FDT_ERROR);
		write_be32(FDT_OFF_DT_STRUCT_OFFSET, UINT32_MAX);
		VERIFY(fdt_init(&tree, buffer, cursor) == FDT_ERROR);
	}

	TEST("reg 32 bit ranges") {
		const uint8_t data[] = {0, 0, 0x10, 0, 0, 0, 2, 0, 0, 0, 0x80, 0, 0, 0, 4, 0};
		fdt_property_t prop = {data, sizeof(data)};
		fdt_reg_iter_t iter;
		fdt_range_t range;
		VERIFY(fdt_reg_iter_init(&iter, &prop, 1, 1) == FDT_SUCCESS);
		VERIFY(fdt_reg_iter_next(&iter, &range) == FDT_SUCCESS);
		VERIFY(range.base == 0x1000 && range.size == 0x200);
		VERIFY(fdt_reg_iter_next(&iter, &range) == FDT_SUCCESS);
		VERIFY(range.base == 0x8000 && range.size == 0x400);
		VERIFY(fdt_reg_iter_next(&iter, &range) == FDT_NO_MORE_REGS);
		VERIFY(fdt_reg_iter_next(&iter, &range) == FDT_NO_MORE_REGS);
	}

	TEST("reg 64 bit range") {
		const uint8_t data[] = {1, 2, 3, 4, 5, 6, 7, 8, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18};
		fdt_property_t prop = {data, sizeof(data)};
		fdt_reg_iter_t iter;
		fdt_range_t range;
		VERIFY(fdt_reg_iter_init(&iter, &prop, 2, 2) == FDT_SUCCESS);
		VERIFY(fdt_reg_iter_next(&iter, &range) == FDT_SUCCESS);
		VERIFY(range.base == UINT64_C(0x0102030405060708));
		VERIFY(range.size == UINT64_C(0x1112131415161718));
		VERIFY(fdt_reg_iter_next(&iter, &range) == FDT_NO_MORE_REGS);
	}

	TEST("reg invalid arguments") {
		const uint8_t data[16] = {0};
		fdt_property_t prop = {data, sizeof(data)}, missing = {0};
		fdt_reg_iter_t iter = {0};
		fdt_range_t range;
		VERIFY(fdt_reg_iter_init(NULL, &prop, 1, 1) == FDT_ERROR);
		VERIFY(fdt_reg_iter_init(&iter, NULL, 1, 1) == FDT_ERROR);
		VERIFY(fdt_reg_iter_init(&iter, &missing, 1, 1) == FDT_ERROR);
		VERIFY(fdt_reg_iter_init(&iter, &prop, 3, 1) == FDT_ERROR);
		VERIFY(fdt_reg_iter_init(&iter, &prop, 1, 3) == FDT_ERROR);
		VERIFY(fdt_reg_iter_init(&iter, &prop, 0, 0) == FDT_ERROR);
		prop.length = 7;
		VERIFY(fdt_reg_iter_init(&iter, &prop, 1, 1) == FDT_ERROR);
		VERIFY(fdt_reg_iter_next(NULL, &range) == FDT_ERROR);
		VERIFY(fdt_reg_iter_next(&iter, &range) == FDT_ERROR);
		prop.length = 0;
		VERIFY(fdt_reg_iter_init(&iter, &prop, 1, 1) == FDT_SUCCESS);
		VERIFY(fdt_reg_iter_next(&iter, NULL) == FDT_ERROR);
		VERIFY(fdt_reg_iter_next(&iter, &range) == FDT_NO_MORE_REGS);
	}

	TEST("memreserve ranges") {
		REQUIRE_TREE();
		fdt_memreserve_iter_t iter;
		fdt_range_t range;
		VERIFY(fdt_memreserve_iter_init(&tree, &iter) == FDT_SUCCESS);
		VERIFY(fdt_memreserve_iter_next(&tree, &iter, &range) == FDT_SUCCESS);
		VERIFY(range.base == UINT64_C(0x123456000) && range.size == 0x2000);
		VERIFY(fdt_memreserve_iter_next(&tree, &iter, &range) == FDT_NO_MORE_RANGES);
		VERIFY(fdt_memreserve_iter_next(&tree, &iter, &range) == FDT_NO_MORE_RANGES);
		memset(buffer + 40, 0, 16);
		VERIFY(fdt_memreserve_iter_init(&tree, &iter) == FDT_SUCCESS);
		VERIFY(fdt_memreserve_iter_next(&tree, &iter, &range) == FDT_NO_MORE_RANGES);
	}

	TEST("memory ranges") {
		REQUIRE_TREE();
		fdt_memory_iter_t iter;
		fdt_range_t range;
		VERIFY(fdt_memory_iter_init(&tree, &iter) == FDT_SUCCESS);
		VERIFY(fdt_memory_iter_next(&iter, &range) == FDT_SUCCESS);
		VERIFY(range.base == 0x1000 && range.size == 0x2000);
		VERIFY(fdt_memory_iter_next(&iter, &range) == FDT_SUCCESS);
		VERIFY(range.base == 0x8000 && range.size == 0x4000);
		VERIFY(fdt_memory_iter_next(&iter, &range) == FDT_NO_MORE_RANGES);
		VERIFY(fdt_memory_iter_next(&iter, &range) == FDT_NO_MORE_RANGES);
	}

	TEST("reserved ranges") {
		REQUIRE_TREE();
		fdt_reserved_iter_t iter;
		fdt_range_t range;
		VERIFY(fdt_reserved_iter_init(&tree, &iter) == FDT_SUCCESS);
		VERIFY(fdt_reserved_iter_next(&iter, &range) == FDT_SUCCESS);
		VERIFY(range.base == 0x9000 && range.size == 0x100);
		VERIFY(fdt_reserved_iter_next(&iter, &range) == FDT_SUCCESS);
		VERIFY(range.base == 0xa000 && range.size == 0x200);
		VERIFY(fdt_reserved_iter_next(&iter, &range) == FDT_NO_MORE_RANGES);
		VERIFY(fdt_reserved_iter_next(&iter, &range) == FDT_NO_MORE_RANGES);
	}

	TEST("range invalid arguments") {
		REQUIRE_TREE();
		fdt_memreserve_iter_t memreserve = {0};
		fdt_memory_iter_t memory_iter = {0};
		fdt_reserved_iter_t reserved_iter = {0};
		fdt_range_t range;
		fdt_tree_t empty = {0};
		VERIFY(fdt_memreserve_iter_init(NULL, &memreserve) == FDT_ERROR);
		VERIFY(fdt_memreserve_iter_init(&empty, &memreserve) == FDT_ERROR);
		VERIFY(fdt_memreserve_iter_init(&tree, NULL) == FDT_ERROR);
		VERIFY(fdt_memreserve_iter_next(NULL, &memreserve, &range) == FDT_ERROR);
		VERIFY(fdt_memreserve_iter_next(&tree, NULL, &range) == FDT_ERROR);
		VERIFY(fdt_memreserve_iter_next(&tree, &memreserve, NULL) == FDT_ERROR);
		VERIFY(fdt_memory_iter_init(NULL, &memory_iter) == FDT_ERROR);
		VERIFY(fdt_memory_iter_init(&empty, &memory_iter) == FDT_ERROR);
		VERIFY(fdt_memory_iter_init(&tree, NULL) == FDT_ERROR);
		VERIFY(fdt_memory_iter_next(NULL, &range) == FDT_ERROR);
		VERIFY(fdt_memory_iter_next(&memory_iter, NULL) == FDT_ERROR);
		VERIFY(fdt_reserved_iter_init(NULL, &reserved_iter) == FDT_ERROR);
		VERIFY(fdt_reserved_iter_init(&empty, &reserved_iter) == FDT_ERROR);
		VERIFY(fdt_reserved_iter_init(&tree, NULL) == FDT_ERROR);
		VERIFY(fdt_reserved_iter_next(NULL, &range) == FDT_ERROR);
		VERIFY(fdt_reserved_iter_next(&reserved_iter, NULL) == FDT_ERROR);
	}

	TEST("Raspberry Pi DTB") {
		uint8_t data[4096 * 80];
		FILE* file = fopen("bcm2712-rpi-b.dtb", "rb");
		if (file == NULL) {
			FAIL("Could not open bcm2712-rpi-b.dtb.");
		}
		size_t size = fread(data, 1, sizeof(data), file);
		int read_error = ferror(file);
		int extra = fgetc(file);
		int close_result = fclose(file);
		VERIFY(!read_error && extra == EOF && close_result == 0);
		fdt_tree_t loaded = {0};
		fdt_node_t node;
		fdt_property_t prop;
		VERIFY(fdt_init(&loaded, data, size) == FDT_SUCCESS);
		VERIFY(fdt_find_node(&loaded, "", &node) == FDT_SUCCESS);
		VERIFY(fdt_get_property(&loaded, &node, "model", &prop) == FDT_SUCCESS);
		VERIFY(prop.length > 1);
		VERIFY(((const char*)prop.data)[prop.length - 1] == '\0');
	}

	TEST("range edge cases") {
		REQUIRE_TREE();
		fdt_memreserve_iter_t reservations;
		fdt_memory_iter_t memory_iter;
		fdt_reserved_iter_t reserved_iter;
		fdt_range_t range;
		fdt_reg_iter_t reg;
		const uint8_t data[] = {0, 0, 0, 1, 0, 0, 0, 2, 0, 0, 0, 3};
		fdt_property_t prop = {data, sizeof(data)};
		VERIFY(fdt_reg_iter_init(&reg, &prop, 2, 1) == FDT_SUCCESS);
		VERIFY(fdt_reg_iter_next(&reg, &range) == FDT_SUCCESS);
		VERIFY(range.base == UINT64_C(0x100000002) && range.size == 3);
		VERIFY(fdt_reg_iter_init(&reg, &prop, 1, 2) == FDT_SUCCESS);
		VERIFY(fdt_reg_iter_next(&reg, &range) == FDT_SUCCESS);
		VERIFY(range.base == 1 && range.size == UINT64_C(0x200000003));
		VERIFY(fdt_reg_iter_init(&reg, &prop, 0, 1) == FDT_ERROR);
		VERIFY(fdt_reg_iter_init(&reg, &prop, 1, 0) == FDT_ERROR);

		write_be32(FDT_OFF_MEM_RSVMAP_OFFSET, 41);
		VERIFY(fdt_memreserve_iter_init(&tree, &reservations) == FDT_ERROR);
		write_be32(FDT_OFF_MEM_RSVMAP_OFFSET, 40);
		write_be32(56, 1); /* Remove the reservation terminator. */
		VERIFY(fdt_memreserve_iter_init(&tree, &reservations) == FDT_SUCCESS);
		VERIFY(fdt_memreserve_iter_next(&tree, &reservations, &range) == FDT_SUCCESS);
		VERIFY(fdt_memreserve_iter_next(&tree, &reservations, &range) == FDT_SUCCESS);
		VERIFY(fdt_memreserve_iter_next(&tree, &reservations, &range) == FDT_ERROR);
		fixture_setup();

		/* Turn the first memory node's device_type into an unavailable status. */
		fdt_property_t type;
		VERIFY(fdt_get_property(&tree, &memory, "device_type", &type) == FDT_SUCCESS);
		size_t offset = (const uint8_t*)type.data - buffer;
		write_be32(offset - 4, STATUS);
		VERIFY(fdt_memory_iter_init(&tree, &memory_iter) == FDT_SUCCESS);
		VERIFY(fdt_memory_iter_next(&memory_iter, &range) == FDT_SUCCESS);
		VERIFY(range.base == 0x8000);
		fixture_setup();
		VERIFY(fdt_get_property(&tree, &memory, "reg", &prop) == FDT_SUCCESS);
		offset = (const uint8_t*)prop.data - buffer;
		write_be32(offset - 8, 0);
		VERIFY(fdt_memory_iter_init(&tree, &memory_iter) == FDT_ERROR);
		fixture_setup();

		buffer[reserved.offset + 4] = 'x';
		VERIFY(fdt_reserved_iter_init(&tree, &reserved_iter) == FDT_NO_MORE_RANGES);
		fixture_setup();
		fdt_node_t child;
		VERIFY(fdt_get_child_node(&tree, &reserved, &child) == FDT_SUCCESS);
		write_be32(child.offset, FDT_END_NODE);
		VERIFY(fdt_reserved_iter_init(&tree, &reserved_iter) == FDT_NO_MORE_RANGES);
	}

	TEST("read header size") {
		REQUIRE_TREE();
		/* Only the header exists here; totalsize describes a larger blob. */
		uint8_t header[FDT_HEADER_SIZE];
		memcpy(header, buffer, sizeof(header));
		size_t total_size = 0;
		VERIFY(fdt_read_header_size(header, sizeof(header), &total_size) == FDT_SUCCESS);
		VERIFY(total_size == cursor && total_size > sizeof(header));
		VERIFY(fdt_read_header_size(NULL, sizeof(header), &total_size) == FDT_ERROR);
		VERIFY(fdt_read_header_size(header, sizeof(header), NULL) == FDT_ERROR);
		for (size_t available = 0; available < sizeof(header); available++) {
			VERIFY(fdt_read_header_size(header, available, &total_size) == FDT_ERROR);
		}
		VERIFY(total_size == cursor);
		header[FDT_MAGIC_OFFSET] = 0;
		VERIFY(fdt_read_header_size(header, sizeof(header), &total_size) == FDT_ERROR);
		memcpy(header, buffer, sizeof(header));
		header[FDT_VERSION_OFFSET + 3] = FDT_VERSION - 1;
		VERIFY(fdt_read_header_size(header, sizeof(header), &total_size) == FDT_ERROR);
		memcpy(header, buffer, sizeof(header));
		header[FDT_LAST_CMP_VERSION_OFFSET + 3] = FDT_VERSION + 1;
		VERIFY(fdt_read_header_size(header, sizeof(header), &total_size) == FDT_ERROR);
		memcpy(header, buffer, sizeof(header));
		memset(header + FDT_TOTALSIZE_OFFSET, 0, 4);
		header[FDT_TOTALSIZE_OFFSET + 3] = FDT_HEADER_SIZE - 1;
		VERIFY(fdt_read_header_size(header, sizeof(header), &total_size) == FDT_ERROR);
		VERIFY(total_size == cursor);
	}
}
