#ifndef FDT_H
#define FDT_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#define FDT_MAGIC 0xd00dfeed
#define FDT_VERSION 17
#define FDT_HEADER_SIZE 40

#define FDT_MAGIC_OFFSET 0
#define FDT_TOTALSIZE_OFFSET 4
#define FDT_OFF_DT_STRUCT_OFFSET 8
#define FDT_OFF_DT_STRING_OFFSET 12
#define FDT_OFF_MEM_RSVMAP_OFFSET 16
#define FDT_VERSION_OFFSET 20
#define FDT_LAST_CMP_VERSION_OFFSET 24
#define FDT_BOOT_CPUID_PHYS_OFFSET 28
#define FDT_SIZE_DT_STRING_OFFSET 32
#define FDT_SIZE_DT_STRUCT_OFFSET 36


typedef struct {
	size_t offset;
} fdt_node_t;

typedef struct {
	const void *data;
	uint32_t length;
} fdt_property_t;

typedef struct {
	const uint8_t* data;
	size_t size;
} fdt_tree_t;

typedef struct {
	const uint8_t* data;
	uint32_t length;
	uint32_t offset;

	uint32_t address_cells;
	uint32_t size_cells;
} fdt_reg_iter_t;

typedef struct {
	fdt_tree_t tree;
	fdt_node_t child;
	fdt_reg_iter_t reg;
} fdt_memory_iter_t;

typedef struct {
	size_t offset;
} fdt_memreserve_iter_t;

typedef struct {
	fdt_tree_t tree;
	fdt_node_t child;
	fdt_reg_iter_t reg;
} fdt_reserved_iter_t;

typedef struct {
	uint64_t base;
	uint64_t size;
} fdt_range_t;

typedef enum {
	FDT_SUCCESS,
	FDT_NO_MORE_NODES,
	FDT_NO_MORE_REGS,
	FDT_NO_MORE_RANGES,
	FDT_NOT_FOUND,
	FDT_ERROR,
} fdt_result_t;

// Reads only FDT_HEADER_SIZE bytes. The declared size may exceed available.
// The caller must make the full blob accessible before calling fdt_init().
fdt_result_t fdt_read_header_size(const void* header, size_t available, size_t* total_size);

fdt_result_t fdt_init(fdt_tree_t* tree, const void* dtb, const size_t dtb_size);

// Finds the first node with this bare name, not a path.
fdt_result_t fdt_find_node(const fdt_tree_t* tree, const char* name, fdt_node_t* node);

fdt_result_t fdt_get_property(const fdt_tree_t* tree, const fdt_node_t* node, const char* name, fdt_property_t* value);

fdt_result_t fdt_get_child_node(const fdt_tree_t* tree, const fdt_node_t* parent, fdt_node_t* node);

fdt_result_t fdt_get_sibling_node(const fdt_tree_t* tree, const fdt_node_t* node, fdt_node_t* sibling);

fdt_result_t fdt_reg_iter_init(fdt_reg_iter_t* iter, const fdt_property_t* reg, uint32_t address_cells, uint32_t size_cells);

fdt_result_t fdt_reg_iter_next(fdt_reg_iter_t* iter, fdt_range_t* out);

fdt_result_t fdt_memory_iter_init(const fdt_tree_t* tree, fdt_memory_iter_t* iter);

fdt_result_t fdt_memory_iter_next(fdt_memory_iter_t* iter, fdt_range_t* out);

fdt_result_t fdt_memreserve_iter_init(const fdt_tree_t* tree, fdt_memreserve_iter_t* iter);

fdt_result_t fdt_memreserve_iter_next(const fdt_tree_t* tree, fdt_memreserve_iter_t* iter, fdt_range_t* out);

fdt_result_t fdt_reserved_iter_init(const fdt_tree_t* tree, fdt_reserved_iter_t* iter);

fdt_result_t fdt_reserved_iter_next(fdt_reserved_iter_t* iter, fdt_range_t* out);

#endif // FDT_H

#ifdef FDT_IMPLEMENTATION
#ifndef FDT_IMPLEMENTATION_INCLUDED
#define FDT_IMPLEMENTATION_INCLUDED

// Each token is exactly 4 bytes
typedef enum {
	FDT_BEGIN_NODE = 0x01, // NUL-terminated name + padding
	FDT_END_NODE   = 0x02, // No extra data
	FDT_PROP       = 0x03, // Length, name offset, value+padding
	FDT_NOP        = 0x04, // No extra data
	FDT_END        = 0x09, // No extra data
} fdt_token_type_t;

typedef struct {
	size_t cursor; // Absolute DTB offset
	size_t end;    // Absolute, exclusive structure block end
} fdt_parser_t;

typedef struct {
	fdt_token_type_t type;
	size_t offset;       // Absolute offset
	const char* name;    // Node name or property name
	const uint8_t* data; // Property value
	uint32_t length;    // Property value length
} fdt_token_t;

static uint32_t fdt_internal_read_be32(const uint8_t* p) {
	return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | ((uint32_t)p[3]);
}

static uint64_t fdt_internal_read_be64(const uint8_t* p) {
	return ((uint64_t)p[0] << 56) | ((uint64_t)p[1] << 48) | ((uint64_t)p[2] << 40) | ((uint64_t)p[3] << 32) | ((uint64_t)p[4] << 24) | ((uint64_t)p[5] << 16) | ((uint64_t)p[6] << 8) | ((uint64_t)p[7]);
}

static int fdt_internal_strcmp(const char* s1, const char* s2) {
	while (*s1 && *s1 == *s2) {
		s1++;
		s2++;
	}
	unsigned char a = (unsigned char)*s1;
	unsigned char b = (unsigned char)*s2;
	return (a > b) - (a < b);
}

// All internal offsets are absolute DTB offsets. Block sizes remain lengths.
static fdt_result_t fdt_internal_get_block(const fdt_tree_t* tree, size_t offset_field, size_t size_field, size_t* start, size_t* end) {
	if (tree == NULL || tree->data == NULL || tree->size < 40) return FDT_ERROR;
	size_t offset = fdt_internal_read_be32(tree->data + offset_field);
	size_t size = fdt_internal_read_be32(tree->data + size_field);
	if (offset < 40 || offset > tree->size || size > tree->size - offset) return FDT_ERROR;
	*start = offset;
	*end = offset + size;
	return FDT_SUCCESS;
}

static const char* fdt_internal_get_name(const fdt_tree_t* tree, size_t offset, size_t end) {
	for (size_t cursor = offset; cursor < end; cursor++) {
		if (tree->data[cursor] == '\0') return (const char*)(tree->data + offset);
	}
	return NULL;
}

static const char* fdt_internal_get_node_name(const fdt_tree_t* tree, size_t offset) {
	size_t start, end;
	if (fdt_internal_get_block(tree, FDT_OFF_DT_STRUCT_OFFSET, FDT_SIZE_DT_STRUCT_OFFSET, &start, &end) != FDT_SUCCESS) return NULL;
	if (offset < start || offset > end || end - offset < 4 || offset % 4 != 0) return NULL;
	if (fdt_internal_read_be32(tree->data + offset) != FDT_BEGIN_NODE) return NULL;
	return fdt_internal_get_name(tree, offset + 4, end);
}

static const char* fdt_internal_get_property_name(const fdt_tree_t* tree, size_t offset) {
	size_t start, end;
	if (fdt_internal_get_block(tree, FDT_OFF_DT_STRING_OFFSET, FDT_SIZE_DT_STRING_OFFSET, &start, &end) != FDT_SUCCESS) return NULL;
	if (offset < start || offset >= end) return NULL;
	return fdt_internal_get_name(tree, offset, end);
}

static fdt_result_t fdt_internal_parser_init(const fdt_tree_t* tree, fdt_parser_t* parser) {
	size_t start, end;
	if (fdt_internal_get_block(tree, FDT_OFF_DT_STRUCT_OFFSET, FDT_SIZE_DT_STRUCT_OFFSET, &start, &end) != FDT_SUCCESS) return FDT_ERROR;
	if (start % 4 != 0 || (end - start) % 4 != 0) return FDT_ERROR;
	parser->cursor = start;
	parser->end = end;
	return FDT_SUCCESS;
}

fdt_result_t fdt_read_header_size(const void* header, size_t available, size_t* total_size) {
	if (header == NULL || total_size == NULL || available < FDT_HEADER_SIZE) return FDT_ERROR;
	const uint8_t* data = (const uint8_t*)header;
	if (fdt_internal_read_be32(data + FDT_MAGIC_OFFSET) != FDT_MAGIC) return FDT_ERROR;
	if (fdt_internal_read_be32(data + FDT_VERSION_OFFSET) != FDT_VERSION) return FDT_ERROR;
	if (fdt_internal_read_be32(data + FDT_LAST_CMP_VERSION_OFFSET) > FDT_VERSION) return FDT_ERROR;
	size_t size = fdt_internal_read_be32(data + FDT_TOTALSIZE_OFFSET);
	if (size < FDT_HEADER_SIZE) return FDT_ERROR;
	*total_size = size;
	return FDT_SUCCESS;
}

static fdt_result_t fdt_internal_validate_header(const fdt_tree_t* tree, const size_t dtb_size) {
	if (tree == NULL || tree->data == NULL || tree->size == 0) return FDT_ERROR;

	size_t total_size;
	if (fdt_read_header_size(tree->data, tree->size, &total_size) != FDT_SUCCESS) return FDT_ERROR;
	if (total_size != dtb_size) return FDT_ERROR;
	fdt_parser_t parser;
	size_t start, end;
	if (fdt_internal_parser_init(tree, &parser) != FDT_SUCCESS) return FDT_ERROR;
	if (fdt_internal_get_block(tree, FDT_OFF_DT_STRING_OFFSET, FDT_SIZE_DT_STRING_OFFSET, &start, &end) != FDT_SUCCESS) return FDT_ERROR;

	return FDT_SUCCESS;
}

static fdt_result_t fdt_internal_next_token(const fdt_tree_t* tree, fdt_parser_t* parser, fdt_token_t* out) {
	if (tree == NULL || parser == NULL || out == NULL) return FDT_ERROR;
	size_t start, end;
	if (fdt_internal_get_block(tree, FDT_OFF_DT_STRUCT_OFFSET, FDT_SIZE_DT_STRUCT_OFFSET, &start, &end) != FDT_SUCCESS) return FDT_ERROR;
	size_t cursor = parser->cursor;
	if (parser->end != end || cursor < start || cursor > end || end - cursor < 4 || cursor % 4 != 0) return FDT_ERROR;

	fdt_token_t token = {0};
	token.offset = cursor;
	token.type = (fdt_token_type_t)fdt_internal_read_be32(tree->data + cursor);
	cursor += 4;

	switch (token.type) {
	case FDT_BEGIN_NODE:
		token.name = fdt_internal_get_node_name(tree, token.offset);
		if (token.name == NULL) return FDT_ERROR;
		while (tree->data[cursor] != '\0') cursor++;
		cursor++;
		break;
	case FDT_PROP:
	{
		if (end - cursor < 8) return FDT_ERROR;
		token.length = fdt_internal_read_be32(tree->data + cursor);
		size_t nameoff = fdt_internal_read_be32(tree->data + cursor + 4);
		cursor += 8;
		if (token.length > end - cursor) return FDT_ERROR;
		token.data = tree->data + cursor;
		cursor += token.length;

		size_t strings_start, strings_end;
		if (fdt_internal_get_block(tree, FDT_OFF_DT_STRING_OFFSET, FDT_SIZE_DT_STRING_OFFSET, &strings_start, &strings_end) != FDT_SUCCESS) return FDT_ERROR;
		if (nameoff >= strings_end - strings_start) return FDT_ERROR;
		// The encoded nameoff is strings-relative; convert it at the boundary.
		token.name = fdt_internal_get_property_name(tree, strings_start + nameoff);
		if (token.name == NULL) return FDT_ERROR;
		break;
	}
	case FDT_END_NODE:
	case FDT_NOP:
	case FDT_END:
		break;
	default:
		return FDT_ERROR;
	}

	size_t padding = (4 - (cursor & 3)) & 3;
	if (padding > end - cursor) return FDT_ERROR;
	parser->cursor = cursor + padding;
	*out = token;
	return FDT_SUCCESS;
}

fdt_result_t fdt_init(fdt_tree_t* tree, const void* dtb, const size_t dtb_size) {
	if (tree == NULL || dtb == NULL) return FDT_ERROR;
	if (dtb_size < FDT_SIZE_DT_STRUCT_OFFSET + sizeof(uint32_t)) return FDT_ERROR;

	fdt_tree_t candidate = { (const uint8_t*)dtb, dtb_size };
	if (fdt_internal_validate_header(&candidate, dtb_size) != FDT_SUCCESS) return FDT_ERROR;

	*tree = candidate;

	return FDT_SUCCESS;
}

fdt_result_t fdt_find_node(const fdt_tree_t* tree, const char* name, fdt_node_t* node) {
	if (tree == NULL || tree->data == NULL || tree->size == 0) return FDT_ERROR;
	if (node == NULL || name == NULL) return FDT_ERROR;

	fdt_parser_t parser;
	if (fdt_internal_parser_init(tree, &parser) != FDT_SUCCESS) return FDT_ERROR;

	while (true) {
		fdt_token_t token;
		if (fdt_internal_next_token(tree, &parser, &token) != FDT_SUCCESS) return FDT_ERROR;
		if (token.type == FDT_END) return FDT_NOT_FOUND;
		if (token.type == FDT_BEGIN_NODE && fdt_internal_strcmp(token.name, name) == 0) {
			node->offset = token.offset;
			return FDT_SUCCESS;
		}
	}
}

fdt_result_t fdt_get_property(const fdt_tree_t* tree, const fdt_node_t* node, const char* name, fdt_property_t* value) {
	if (tree == NULL || tree->data == NULL || tree->size == 0) return FDT_ERROR;
	if (node == NULL || node->offset == 0 || name == NULL || value == NULL) return FDT_ERROR;

	fdt_parser_t parser;
	fdt_token_t token;
	if (fdt_internal_parser_init(tree, &parser) != FDT_SUCCESS) return FDT_ERROR;
	parser.cursor = node->offset;
	if (fdt_internal_next_token(tree, &parser, &token) != FDT_SUCCESS || token.type != FDT_BEGIN_NODE) return FDT_ERROR;

	while (true) {
		if (fdt_internal_next_token(tree, &parser, &token) != FDT_SUCCESS) return FDT_ERROR;
		if (token.type == FDT_BEGIN_NODE || token.type == FDT_END_NODE) return FDT_NOT_FOUND;
		if (token.type == FDT_END) return FDT_ERROR;
		if (token.type == FDT_PROP && fdt_internal_strcmp(token.name, name) == 0) {
			value->data = token.data;
			value->length = token.length;
			return FDT_SUCCESS;
		}
	}
}

fdt_result_t fdt_get_child_node(const fdt_tree_t* tree, const fdt_node_t* parent, fdt_node_t* node) {
	if (tree == NULL || tree->data == NULL || tree->size == 0) return FDT_ERROR;
	if (parent == NULL || parent->offset == 0) return FDT_ERROR;
	if (node == NULL) return FDT_ERROR;

	fdt_parser_t parser;
	fdt_token_t token;
	if (fdt_internal_parser_init(tree, &parser) != FDT_SUCCESS) return FDT_ERROR;
	parser.cursor = parent->offset;
	if (fdt_internal_next_token(tree, &parser, &token) != FDT_SUCCESS || token.type != FDT_BEGIN_NODE) return FDT_ERROR;

	while (true) {
		if (fdt_internal_next_token(tree, &parser, &token) != FDT_SUCCESS) return FDT_ERROR;
		if (token.type == FDT_END_NODE) return FDT_NO_MORE_NODES;
		if (token.type == FDT_END) return FDT_ERROR;
		if (token.type == FDT_BEGIN_NODE) {
			node->offset = token.offset;
			return FDT_SUCCESS;
		}
	}
}

fdt_result_t fdt_get_sibling_node(const fdt_tree_t* tree, const fdt_node_t* node, fdt_node_t* sibling) {
	if (tree == NULL || tree->data == NULL || tree->size == 0) return FDT_ERROR;
	if (node == NULL || node->offset == 0) return FDT_ERROR;
	if (sibling == NULL) return FDT_ERROR;

	fdt_parser_t parser;
	fdt_token_t token;
	if (fdt_internal_parser_init(tree, &parser) != FDT_SUCCESS) return FDT_ERROR;
	parser.cursor = node->offset;
	if (fdt_internal_next_token(tree, &parser, &token) != FDT_SUCCESS || token.type != FDT_BEGIN_NODE) return FDT_ERROR;

	size_t depth = 0;
	bool end_found = false;

	while (true) {
		if (fdt_internal_next_token(tree, &parser, &token) != FDT_SUCCESS) return FDT_ERROR;

		switch (token.type) {
		case FDT_BEGIN_NODE:
		{
			if (end_found) {
				sibling->offset = token.offset;
				return FDT_SUCCESS;
			}
			depth += 1;
			break;
		}
		case FDT_END_NODE:
			if (end_found) return FDT_NO_MORE_NODES;
			if (depth == 0) end_found = true;
			else depth -= 1;

			break;
		case FDT_PROP:
		{
			if (end_found) return FDT_ERROR;
			break;
		}
		case FDT_NOP:
			break;
		case FDT_END:
			if (!end_found) return FDT_ERROR;
			return FDT_NO_MORE_NODES;
		default:
			return FDT_ERROR;
		}
	}
}

fdt_result_t fdt_reg_iter_init(fdt_reg_iter_t* iter, const fdt_property_t* reg, uint32_t address_cells, uint32_t size_cells) {
	if (iter == NULL) return FDT_ERROR;
	if (reg == NULL || reg->data == NULL) return FDT_ERROR;
	if (size_cells < 1 || size_cells > 2 || address_cells < 1 || address_cells > 2) return FDT_ERROR;

	uint32_t entry_size = (address_cells + size_cells) * 4;
	if (entry_size == 0) return FDT_ERROR;
	if (reg->length % entry_size != 0) return FDT_ERROR;

	iter->size_cells = size_cells;
	iter->address_cells = address_cells;
	iter->length = reg->length;
	iter->data = (const uint8_t*)reg->data;
	iter->offset = 0;

	return FDT_SUCCESS;
}

fdt_result_t fdt_reg_iter_next(fdt_reg_iter_t* iter, fdt_range_t* out) {
	if (iter == NULL || iter->data == NULL || out == NULL) return FDT_ERROR;
	if (iter->size_cells < 1 || iter->size_cells > 2 || iter->address_cells < 1 || iter->address_cells > 2) return FDT_ERROR;

	uint32_t entry_size = (iter->address_cells + iter->size_cells) * 4;
	if (entry_size == 0) return FDT_ERROR;
	if (iter->length % entry_size != 0) return FDT_ERROR;

	if (iter->offset >= iter->length) return FDT_NO_MORE_REGS;

	uint64_t address;
	uint64_t size;
	if (iter->address_cells == 1) address = fdt_internal_read_be32(iter->data + iter->offset);
	else address = ((uint64_t)fdt_internal_read_be32(iter->data + iter->offset) << 32) | (uint64_t)(fdt_internal_read_be32(iter->data + iter->offset + 4));
	if (iter->size_cells == 1) size = fdt_internal_read_be32(iter->data + iter->offset + iter->address_cells * 4);
	else size = ((uint64_t)fdt_internal_read_be32(iter->data + iter->offset + iter->address_cells * 4) << 32) | (uint64_t)(fdt_internal_read_be32(iter->data + iter->offset + iter->address_cells * 4 + 4));

	out->base = address;
	out->size = size;
	iter->offset += entry_size;

	return FDT_SUCCESS;
}

static fdt_result_t fdt_internal_node_available(const fdt_tree_t* tree, const fdt_node_t* node, bool* available) {
	fdt_property_t status;
	fdt_result_t result = fdt_get_property(tree, node, "status", &status);
	if (result != FDT_SUCCESS && result != FDT_NOT_FOUND) return result;
	*available = result == FDT_NOT_FOUND;
	if (result == FDT_NOT_FOUND) return FDT_SUCCESS;
	if (status.length == 0 || ((const char*)status.data)[status.length - 1] != '\0') return FDT_ERROR;
	*available = (status.length == sizeof("okay") && fdt_internal_strcmp(status.data, "okay") == 0) || (status.length == sizeof("ok") && fdt_internal_strcmp(status.data, "ok") == 0);
	return FDT_SUCCESS;
}

fdt_result_t fdt_memory_iter_init(const fdt_tree_t* tree, fdt_memory_iter_t* iter) {
	if (tree == NULL || tree->data == NULL || tree->size == 0) return FDT_ERROR;
	if (iter == NULL) return FDT_ERROR;
	fdt_result_t result;

	fdt_node_t root;
	if ((result = fdt_find_node(tree, "", &root)) != FDT_SUCCESS) return result;

	fdt_property_t address_cells;
	if ((result = fdt_get_property(tree, &root, "#address-cells", &address_cells)) != FDT_SUCCESS) return result;

	fdt_property_t size_cells;
	if ((result = fdt_get_property(tree, &root, "#size-cells", &size_cells)) != FDT_SUCCESS) return result;
	if (address_cells.length != 4 || size_cells.length != 4) return FDT_ERROR;

	fdt_node_t node;
	result = fdt_get_child_node(tree, &root, &node);
	if (result == FDT_NO_MORE_NODES) return FDT_NO_MORE_RANGES;
	if (result != FDT_SUCCESS) return result;

	fdt_node_t sibling;
	fdt_property_t reg;
	while (true) {
		bool available;
		if ((result = fdt_internal_node_available(tree, &node, &available)) != FDT_SUCCESS) return result;
		if (!available) {
			result = fdt_get_sibling_node(tree, &node, &node);
			if (result == FDT_NO_MORE_NODES) return FDT_NO_MORE_RANGES;
			if (result != FDT_SUCCESS) return result;
			continue;
		}
		if ((result = fdt_get_property(tree, &node, "device_type", &reg)) == FDT_SUCCESS) {
			const char* device_type = (const char*)reg.data;
			if (reg.length == sizeof("memory") && device_type[reg.length - 1] == '\0' &&
				fdt_internal_strcmp(device_type, "memory") == 0) break;
		} else if (result != FDT_NOT_FOUND) {
			return result;
		}

		result = fdt_get_sibling_node(tree, &node, &sibling);
		if (result == FDT_NO_MORE_NODES) return FDT_NO_MORE_RANGES;
		if (result != FDT_SUCCESS) return result;
		node = sibling;
	}

	if ((result = fdt_get_property(tree, &node, "reg", &reg)) != FDT_SUCCESS) return result;
	if (reg.length == 0) return FDT_ERROR;

	iter->tree = *tree;
	iter->child = node;

	result = fdt_reg_iter_init(&iter->reg, &reg, fdt_internal_read_be32((const uint8_t*)address_cells.data), fdt_internal_read_be32((const uint8_t*)size_cells.data));

	return result;
}

fdt_result_t fdt_memory_iter_next(fdt_memory_iter_t* iter, fdt_range_t* out) {
	if (iter == NULL) return FDT_ERROR;
	if (out == NULL) return FDT_ERROR;

	fdt_result_t result;
	(result = fdt_reg_iter_next(&iter->reg, out));
	if (result == FDT_SUCCESS) return FDT_SUCCESS;
	if (result != FDT_NO_MORE_REGS) return result;

	fdt_node_t node = iter->child;
	fdt_node_t sibling;
	fdt_property_t reg;
	while (true) {
		result = fdt_get_sibling_node(&iter->tree, &node, &sibling);
		if (result == FDT_NO_MORE_NODES) return FDT_NO_MORE_RANGES;
		if (result != FDT_SUCCESS) return result;
		node = sibling;

		bool available;
		if ((result = fdt_internal_node_available(&iter->tree, &node, &available)) != FDT_SUCCESS) return result;
		if (!available) continue;
		if ((result = fdt_get_property(&iter->tree, &node, "device_type", &reg)) == FDT_SUCCESS) {
			const char* device_type = (const char*)reg.data;
			if (reg.length == sizeof("memory") && device_type[reg.length - 1] == '\0' &&
				fdt_internal_strcmp(device_type, "memory") == 0) break;
		} else if (result != FDT_NOT_FOUND) {
			return result;
		}
	}

	if ((result = fdt_get_property(&iter->tree, &node, "reg", &reg)) != FDT_SUCCESS) return result;
	if (reg.length == 0) return FDT_ERROR;

	iter->child = node;
	if ((result = fdt_reg_iter_init(&iter->reg, &reg, iter->reg.address_cells, iter->reg.size_cells)) != FDT_SUCCESS) return result;

	return fdt_reg_iter_next(&iter->reg, out);
}

static fdt_result_t fdt_internal_memreserve_bounds(const fdt_tree_t* tree, size_t* start, size_t* end) {
	if (tree == NULL || tree->data == NULL || tree->size < 40) return FDT_ERROR;
	*start = fdt_internal_read_be32(tree->data + FDT_OFF_MEM_RSVMAP_OFFSET);
	*end = tree->size;
	if (*start < 40 || *start % 8 != 0 || *start > *end) return FDT_ERROR;
	size_t block_start, block_end;
	const size_t offsets[] = { FDT_OFF_DT_STRUCT_OFFSET, FDT_OFF_DT_STRING_OFFSET };
	const size_t sizes[] = { FDT_SIZE_DT_STRUCT_OFFSET, FDT_SIZE_DT_STRING_OFFSET };
	for (size_t i = 0; i < 2; i++) {
		if (fdt_internal_get_block(tree, offsets[i], sizes[i], &block_start, &block_end) != FDT_SUCCESS) return FDT_ERROR;
		if (block_start <= *start && *start < block_end) return FDT_ERROR;
		if (block_start >= *start && block_start < *end) *end = block_start;
	}
	if (*end - *start < 16) return FDT_ERROR;
	return FDT_SUCCESS;
}

fdt_result_t fdt_memreserve_iter_init(const fdt_tree_t* tree, fdt_memreserve_iter_t* iter) {
	if (tree == NULL || tree->data == NULL || tree->size == 0) return FDT_ERROR;
	if (iter == NULL) return FDT_ERROR;

	size_t start, end;
	if (fdt_internal_memreserve_bounds(tree, &start, &end) != FDT_SUCCESS) return FDT_ERROR;
	iter->offset = start;

	return FDT_SUCCESS;
}

fdt_result_t fdt_memreserve_iter_next(const fdt_tree_t* tree, fdt_memreserve_iter_t* iter, fdt_range_t* out) {
	if (tree == NULL || tree->data == NULL || tree->size == 0) return FDT_ERROR;
	if (iter == NULL) return FDT_ERROR;
	if (out == NULL) return FDT_ERROR;

	size_t start, end;
	if (fdt_internal_memreserve_bounds(tree, &start, &end) != FDT_SUCCESS) return FDT_ERROR;
	if (iter->offset < start || iter->offset > end || end - iter->offset < 16 || (iter->offset - start) % 16 != 0) return FDT_ERROR;
	out->base = fdt_internal_read_be64(tree->data + iter->offset);
	out->size = fdt_internal_read_be64(tree->data + iter->offset + 8);

	if (out->base == 0 && out->size == 0) return FDT_NO_MORE_RANGES;

	iter->offset += 16;

	return FDT_SUCCESS;
}

fdt_result_t fdt_reserved_iter_init(const fdt_tree_t* tree, fdt_reserved_iter_t* iter) {
	if (tree == NULL || tree->data == NULL || tree->size == 0) return FDT_ERROR;
	if (iter == NULL) return FDT_ERROR;
	fdt_result_t result;

	fdt_node_t root;
	fdt_node_t parent;
	if ((result = fdt_find_node(tree, "", &root)) != FDT_SUCCESS) return result;
	result = fdt_get_child_node(tree, &root, &parent);
	if (result == FDT_NO_MORE_NODES) return FDT_NO_MORE_RANGES;
	if (result != FDT_SUCCESS) return result;
	while (true) {
		const char* name = fdt_internal_get_node_name(tree, parent.offset);
		if (name == NULL) return FDT_ERROR;
		if (fdt_internal_strcmp(name, "reserved-memory") == 0) break;
		result = fdt_get_sibling_node(tree, &parent, &parent);
		if (result == FDT_NO_MORE_NODES) return FDT_NO_MORE_RANGES;
		if (result != FDT_SUCCESS) return result;
	}

	fdt_node_t node;
	result = fdt_get_child_node(tree, &parent, &node);
	if (result == FDT_NO_MORE_NODES) return FDT_NO_MORE_RANGES;
	if (result != FDT_SUCCESS) return result;

	fdt_property_t address_cells;
	if ((result = fdt_get_property(tree, &parent, "#address-cells", &address_cells)) != FDT_SUCCESS) return result;

	fdt_property_t size_cells;
	if ((result = fdt_get_property(tree, &parent, "#size-cells", &size_cells)) != FDT_SUCCESS) return result;
	if (address_cells.length != 4 || size_cells.length != 4) return FDT_ERROR;

	fdt_node_t sibling;
	fdt_property_t prop;
	while (true) {
		if ((result = fdt_get_property(tree, &node, "status", &prop)) == FDT_SUCCESS) {
			const char* status = (const char*)prop.data;
			fdt_result_t result2;
			if (prop.length == sizeof("disabled") && status[prop.length - 1] == '\0' && 
				fdt_internal_strcmp(status, "disabled") == 0) {

				result2 = fdt_get_sibling_node(tree, &node, &sibling);
				if (result2 == FDT_NO_MORE_NODES) return FDT_NO_MORE_RANGES;
				if (result2 != FDT_SUCCESS) return result2;
				node = sibling;
				continue;
			}
		}
		if (result != FDT_SUCCESS && result != FDT_NOT_FOUND) return result;

		if ((result = fdt_get_property(tree, &node, "reg", &prop)) == FDT_SUCCESS) break;
		if (result != FDT_NOT_FOUND) return result;

		result = fdt_get_sibling_node(tree, &node, &sibling);
		if (result == FDT_NO_MORE_NODES) return FDT_NO_MORE_RANGES;
		if (result != FDT_SUCCESS) return result;
		node = sibling;
	}

	iter->tree = *tree;
	iter->child = node;

	if (prop.length == 0) return FDT_ERROR;
	result = fdt_reg_iter_init(&iter->reg, &prop, fdt_internal_read_be32((const uint8_t*)address_cells.data), fdt_internal_read_be32((const uint8_t*)size_cells.data));

	return result;
}

fdt_result_t fdt_reserved_iter_next(fdt_reserved_iter_t* iter, fdt_range_t* out) {
	if (iter == NULL) return FDT_ERROR;
	if (out == NULL) return FDT_ERROR;

	fdt_result_t result;
	(result = fdt_reg_iter_next(&iter->reg, out));
	if (result == FDT_SUCCESS) return FDT_SUCCESS;
	if (result != FDT_NO_MORE_REGS) return result;

	fdt_node_t node = iter->child;
	fdt_node_t sibling;
	fdt_property_t prop;
	while (true) {
		result = fdt_get_sibling_node(&iter->tree, &node, &sibling);
		if (result == FDT_NO_MORE_NODES) return FDT_NO_MORE_RANGES;
		if (result != FDT_SUCCESS) return result;
		node = sibling;

		if ((result = fdt_get_property(&iter->tree, &node, "status", &prop)) == FDT_SUCCESS) {
			const char* status = (const char*)prop.data;
			if (prop.length == sizeof("disabled") && status[prop.length - 1] == '\0' && 
				fdt_internal_strcmp(status, "disabled") == 0) {
				continue;
			}
		}
		if (result != FDT_SUCCESS && result != FDT_NOT_FOUND) return result;

		if ((result = fdt_get_property(&iter->tree, &node, "reg", &prop)) == FDT_SUCCESS) break;
		if (result != FDT_NOT_FOUND) return result;
	}

	iter->child = node;
	if (prop.length == 0) return FDT_ERROR;
	if ((result = fdt_reg_iter_init(&iter->reg, &prop, iter->reg.address_cells, iter->reg.size_cells)) != FDT_SUCCESS) return result;

	return fdt_reg_iter_next(&iter->reg, out);
}

#endif // FDT_IMPLEMENTATION_INCLUDED
#endif // FDT_IMPLEMENTATION
