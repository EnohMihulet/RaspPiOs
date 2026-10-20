#ifndef FREE_LIST_H
#define FREE_LIST_H
/* Define FREE_LIST_IMPLEMENTATION in exactly one translation unit. C99 or later.
 * Uses caller-owned storage; no malloc/free or other libc calls are required.
 * Requires a flat address space with uintptr_t address ordering.
 * Keep the allocator and arena alive; serialize access, including from interrupts.
 * Metadata fields are for inspection; modify them through the API.
 *
 * Define configuration macros before inclusion, consistently across translation
 * units. FREE_LIST_ALIGNMENT defaults to 16 and must be a power of two suitable
 * for block_t and the types you allocate. FREE_LIST_MIN_PAYLOAD_SIZE defaults to
 * 16 and is rounded up to that alignment. FREE_LIST_MIN_ARENA_SIZE is derived.
 * FREE_LIST_VALIDATE_EVERY_OPERATION defaults to 1. Set it to 0 to skip the full
 * physical walk on each operation; free-list and touched-header checks remain.
 * freelist_validate always checks all invariants. Full validation is O(blocks).
 *
 * Except for the container callbacks, public functions return FREELIST_OK on
 * success. Failure leaves the arena, allocator, and outputs unchanged.
 * Except for init, supply an initialized or
 * zero-initialized allocator in readable storage. Its arena must remain valid.
 * Output parameters must point to writable void* objects outside the allocator
 * and arena. Zero-size allocation succeeds with a NULL output.
 */

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifndef FREE_LIST_ALIGNMENT
#define FREE_LIST_ALIGNMENT 16
#endif
#ifndef FREE_LIST_MIN_PAYLOAD_SIZE
#define FREE_LIST_MIN_PAYLOAD_SIZE 16
#endif
#ifndef FREE_LIST_VALIDATE_EVERY_OPERATION
#define FREE_LIST_VALIDATE_EVERY_OPERATION 1
#endif

#if FREE_LIST_ALIGNMENT <= 0 || (FREE_LIST_ALIGNMENT & (FREE_LIST_ALIGNMENT - 1)) != 0
#error "FREE_LIST_ALIGNMENT must be a positive power of two"
#endif
#if FREE_LIST_ALIGNMENT > SIZE_MAX || FREE_LIST_ALIGNMENT > UINTPTR_MAX
#error "FREE_LIST_ALIGNMENT is too large"
#endif
#if FREE_LIST_MIN_PAYLOAD_SIZE <= 0 || FREE_LIST_MIN_PAYLOAD_SIZE > SIZE_MAX - (FREE_LIST_ALIGNMENT - 1)
#error "FREE_LIST_MIN_PAYLOAD_SIZE cannot be rounded to the alignment"
#endif
#if FREE_LIST_VALIDATE_EVERY_OPERATION != 0 && FREE_LIST_VALIDATE_EVERY_OPERATION != 1
#error "FREE_LIST_VALIDATE_EVERY_OPERATION must be 0 or 1"
#endif

#define FREE_LIST_ALLOCATOR_MAGIC 0xC4A71F93u
#define FREE_LIST_BLOCK_MAGIC 0xDEADBEEFu

typedef struct block {
	size_t size;                // Usable payload capacity
	struct block* next_free;
	uint32_t magic;
	unsigned char free;         // 0 or 1; other values indicate corruption
} block_t;

#define FREE_LIST_HEADER_SIZE ((sizeof(block_t) + (size_t)FREE_LIST_ALIGNMENT - 1) & ~((size_t)FREE_LIST_ALIGNMENT - 1))
#define FREE_LIST_INTERNAL_MIN_PAYLOAD_SIZE \
	(((size_t)FREE_LIST_MIN_PAYLOAD_SIZE + (size_t)FREE_LIST_ALIGNMENT - 1) & ~((size_t)FREE_LIST_ALIGNMENT - 1))
#define FREE_LIST_MIN_ARENA_SIZE (FREE_LIST_HEADER_SIZE + FREE_LIST_INTERNAL_MIN_PAYLOAD_SIZE)

/* C99 compile-time checks, including the header's native alignment. */
typedef struct {
	char padding;
	block_t block;
} freelist_internal_alignment_probe;
typedef char freelist_internal_alignment_check[
	FREE_LIST_ALIGNMENT % offsetof(freelist_internal_alignment_probe, block) == 0 ? 1 : -1];
typedef char freelist_internal_header_size_check[
	sizeof(block_t) <= SIZE_MAX - (FREE_LIST_ALIGNMENT - 1) ? 1 : -1];
typedef char freelist_internal_arena_size_check[
	FREE_LIST_INTERNAL_MIN_PAYLOAD_SIZE <= SIZE_MAX - FREE_LIST_HEADER_SIZE ? 1 : -1];

typedef struct {
	block_t* head;
	void* arena_start;
	size_t arena_size;
	uint32_t magic;
} freelist_allocator;

typedef enum {
	FREELIST_OK,
	FREELIST_INVALID_POINTER,
	FREELIST_DOUBLE_FREE,
	FREELIST_CORRUPTED,
	FREELIST_INVALID_ARGUMENT,
	FREELIST_OUT_OF_MEMORY,
	FREELIST_OVERFLOW
} freelist_result;

/* Align the start upward and trim the end downward. The remaining arena must
 * hold FREE_LIST_MIN_ARENA_SIZE bytes. alloc must be outside the supplied buffer.
 * alloc may be uninitialized; successful reinitialization discards old allocations.
 */
freelist_result freelist_init(freelist_allocator* alloc, void* memory, size_t size);

/* Read-only check of the physical blocks, free-list membership/order, and coalescing. */
freelist_result freelist_validate(const freelist_allocator* alloc);

/* First-fit allocation; nonzero requests get at least the minimum payload size. */
freelist_result freelist_alloc(freelist_allocator* alloc, size_t size, void** out);

/* NULL is a successful no-op. Other pointers must be exact live payload starts.
 * A repeated free reports DOUBLE_FREE if its header survives coalescing, otherwise
 * INVALID_POINTER. A stale pointer cannot be distinguished from a reused allocation.
 */
freelist_result freelist_dealloc(freelist_allocator* alloc, void* ptr);

/* Allocate count * size bytes and zero them. Check multiplication for overflow. */
freelist_result freelist_calloc(freelist_allocator* alloc, size_t count, size_t size, void** out);

/* NULL behaves like allocate; size == 0 frees ptr and sets *out to NULL.
 * Try resizing in place, otherwise move and preserve the existing bytes.
 * Failure preserves the original allocation. Newly added bytes are uninitialized.
 */
freelist_result freelist_realloc(freelist_allocator* alloc, void* ptr, size_t size, void** out);

/* container_allocator-compatible callbacks. context points to an initialized
 * freelist_allocator; it and its arena must outlive all allocations.
 * FREE_LIST_ALIGNMENT must satisfy the container's element alignment.
 * Allocation returns NULL on failure or for zero bytes. Deallocation retains
 * validation but discards the result; use freelist_dealloc for diagnostics.
 */
void* freelist_container_allocate(void* context, size_t bytes);
void freelist_container_deallocate(void* context, void* memory);

#endif // FREE_LIST_H

#ifdef FREE_LIST_IMPLEMENTATION
#ifndef FREE_LIST_IMPLEMENTATION_INCLUDED
#define FREE_LIST_IMPLEMENTATION_INCLUDED

static uintptr_t freelist_internal_addr(const void* ptr) {
	return (uintptr_t)ptr;
}

static bool freelist_internal_overlaps(uintptr_t a, size_t a_size, uintptr_t b, size_t b_size) {
	return a <= b ? b - a < a_size : a - b < b_size;
}

static freelist_result freelist_internal_check_allocator(const freelist_allocator* alloc) {
	if (!alloc) return FREELIST_INVALID_ARGUMENT;
	if (alloc->magic != FREE_LIST_ALLOCATOR_MAGIC || !alloc->arena_start) return FREELIST_CORRUPTED;

	uintptr_t start = freelist_internal_addr(alloc->arena_start);

	if (start % FREE_LIST_ALIGNMENT || alloc->arena_size % FREE_LIST_ALIGNMENT)
		return FREELIST_CORRUPTED;
	if (alloc->arena_size < FREE_LIST_MIN_ARENA_SIZE || alloc->arena_size > UINTPTR_MAX - start)
		return FREELIST_CORRUPTED;
	if (freelist_internal_overlaps(freelist_internal_addr(alloc), sizeof(*alloc), start, alloc->arena_size))
		return FREELIST_CORRUPTED;

	return FREELIST_OK;
}

/* Check bounds and alignment before reading any header fields. */
static freelist_result freelist_internal_check_block(const freelist_allocator* alloc, const block_t* block) {
	uintptr_t start = freelist_internal_addr(alloc->arena_start);
	uintptr_t end = start + alloc->arena_size;
	uintptr_t current = freelist_internal_addr(block);

	if (current < start || current >= end || current % FREE_LIST_ALIGNMENT) return FREELIST_CORRUPTED;

	size_t remaining = (size_t)(end - current);

	if (remaining < FREE_LIST_MIN_ARENA_SIZE) return FREELIST_CORRUPTED;
	if (block->magic != FREE_LIST_BLOCK_MAGIC || block->free > 1) return FREELIST_CORRUPTED;
	if (block->size % FREE_LIST_ALIGNMENT) return FREELIST_CORRUPTED;
	if (block->size < FREE_LIST_INTERNAL_MIN_PAYLOAD_SIZE || block->size > remaining - FREE_LIST_HEADER_SIZE) return FREELIST_CORRUPTED;
	if (!block->free && block->next_free) return FREELIST_CORRUPTED;
	return FREELIST_OK;
}

freelist_result freelist_validate(const freelist_allocator* alloc) {
	freelist_result result = freelist_internal_check_allocator(alloc);

	if (result != FREELIST_OK) return result;

	uintptr_t current = freelist_internal_addr(alloc->arena_start);
	uintptr_t end = current + alloc->arena_size;
	const block_t* next_free = alloc->head;
	bool previous_free = false;

	while (current < end) {
		const block_t* block = (const block_t*)current;
		result = freelist_internal_check_block(alloc, block);

		if (result != FREELIST_OK) return result;
		if (block->free) {
			if (previous_free || next_free != block) return FREELIST_CORRUPTED;
			next_free = block->next_free;
		}

		previous_free = block->free != 0;
		current += FREE_LIST_HEADER_SIZE + block->size;
	}
	return current == end && !next_free ? FREELIST_OK : FREELIST_CORRUPTED;
}

static freelist_result freelist_internal_check(const freelist_allocator* alloc) {
#if FREE_LIST_VALIDATE_EVERY_OPERATION
	return freelist_validate(alloc);
#else
	freelist_result result = freelist_internal_check_allocator(alloc);

	if (result != FREELIST_OK) return result;

	const block_t* block = alloc->head;

	while (block) {
		result = freelist_internal_check_block(alloc, block);

		if (result != FREELIST_OK) return result;
		if (!block->free) return FREELIST_CORRUPTED;

		uintptr_t end = freelist_internal_addr(block) + FREE_LIST_HEADER_SIZE + block->size;

		if (block->next_free && freelist_internal_addr(block->next_free) <= end) return FREELIST_CORRUPTED;

		block = block->next_free;
	}

	return FREELIST_OK;
#endif
}

static freelist_result freelist_internal_check_output(const freelist_allocator* alloc, void** out) {
	if (!out) return FREELIST_INVALID_ARGUMENT;

	uintptr_t address = freelist_internal_addr(out);

	if (sizeof(*out) > UINTPTR_MAX - address) return FREELIST_INVALID_ARGUMENT;
	if (freelist_internal_overlaps(address, sizeof(*out), freelist_internal_addr(alloc), sizeof(*alloc)))
		return FREELIST_INVALID_ARGUMENT;
	if (freelist_internal_overlaps(address, sizeof(*out), freelist_internal_addr(alloc->arena_start), alloc->arena_size))
		return FREELIST_INVALID_ARGUMENT;

	return FREELIST_OK;
}

static freelist_result freelist_internal_request_size(size_t size, size_t* out) {
	if (size < FREE_LIST_INTERNAL_MIN_PAYLOAD_SIZE) size = FREE_LIST_INTERNAL_MIN_PAYLOAD_SIZE;
	if (size > SIZE_MAX - (FREE_LIST_ALIGNMENT - 1)) return FREELIST_OVERFLOW;

	*out = (size + FREE_LIST_ALIGNMENT - 1) & ~((size_t)FREE_LIST_ALIGNMENT - 1);

	return FREELIST_OK;
}

static freelist_result freelist_internal_find_block(freelist_allocator* alloc, const void* ptr, block_t** out) {
	uintptr_t target = freelist_internal_addr(ptr);
	uintptr_t current = freelist_internal_addr(alloc->arena_start);
	uintptr_t end = current + alloc->arena_size;

	if (target < current || target >= end || target % FREE_LIST_ALIGNMENT) return FREELIST_INVALID_POINTER;

	while (current < end) {
		block_t* block = (block_t*)current;
		freelist_result result = freelist_internal_check_block(alloc, block);

		if (result != FREELIST_OK) return result;

		uintptr_t payload = current + FREE_LIST_HEADER_SIZE;

		if (target == payload) {
			if (block->free) return FREELIST_DOUBLE_FREE;
			*out = block;
			return FREELIST_OK;
		}

		if (target < payload + block->size) return FREELIST_INVALID_POINTER;

		current = payload + block->size;
	}
	return FREELIST_INVALID_POINTER;
}

static void freelist_internal_set_block(block_t* block, size_t size, block_t* next, unsigned char free) {
	block->size = size;
	block->next_free = next;
	block->magic = FREE_LIST_BLOCK_MAGIC;
	block->free = free;
}

/* Mutation helpers operate only on checked state and cannot fail. */
static block_t* freelist_internal_take(freelist_allocator* alloc, size_t requested) {
	block_t** link = &alloc->head;

	while (*link) {
		block_t* block = *link;

		if (block->size < requested) {
			link = &block->next_free;
			continue;
		}
		if (block->size - requested >= FREE_LIST_MIN_ARENA_SIZE) {
			block_t* rest = (block_t*)((unsigned char*)block + FREE_LIST_HEADER_SIZE + requested);
			freelist_internal_set_block(rest, block->size - requested - FREE_LIST_HEADER_SIZE, block->next_free, 1);
			*link = rest;
			block->size = requested;
		}
		else *link = block->next_free;

		block->next_free = NULL;
		block->free = 0;

		return block;
	}

	return NULL;
}

static void freelist_internal_release(freelist_allocator* alloc, block_t* block) {
	block_t* previous = NULL;
	block_t* next = alloc->head;

	while (next && freelist_internal_addr(next) < freelist_internal_addr(block)) {
		previous = next;
		next = next->next_free;
	}

	block->free = 1;
	block->next_free = next;

	if (next && (unsigned char*)block + FREE_LIST_HEADER_SIZE + block->size == (unsigned char*)next) {
		block->size += FREE_LIST_HEADER_SIZE + next->size;
		block->next_free = next->next_free;
		next->magic = 0;
	}
	if (previous && (unsigned char*)previous + FREE_LIST_HEADER_SIZE + previous->size == (unsigned char*)block) {
		previous->size += FREE_LIST_HEADER_SIZE + block->size;
		previous->next_free = block->next_free;
		block->magic = 0;
	}
	else if (previous) previous->next_free = block;
	else alloc->head = block;
}

static void freelist_internal_trim(freelist_allocator* alloc, block_t* block, size_t requested) {
	if (block->size - requested < FREE_LIST_MIN_ARENA_SIZE) return;

	block_t* rest = (block_t*)((unsigned char*)block + FREE_LIST_HEADER_SIZE + requested);

	freelist_internal_set_block(rest, block->size - requested - FREE_LIST_HEADER_SIZE, NULL, 0);

	block->size = requested;

	freelist_internal_release(alloc, rest);
}

static freelist_result freelist_internal_allocate(freelist_allocator* alloc, size_t size, void** out) {
	if (size == 0) {
		*out = NULL;
		return FREELIST_OK;
	}

	size_t requested;
	freelist_result result = freelist_internal_request_size(size, &requested);

	if (result != FREELIST_OK) return result;

	block_t* block = freelist_internal_take(alloc, requested);

	if (!block) return FREELIST_OUT_OF_MEMORY;

	*out = (unsigned char*)block + FREE_LIST_HEADER_SIZE;

	return FREELIST_OK;
}

freelist_result freelist_init(freelist_allocator* alloc, void* memory, size_t size) {
	if (!alloc || !memory) return FREELIST_INVALID_ARGUMENT;

	uintptr_t original = freelist_internal_addr(memory);

	if (size > UINTPTR_MAX - original) return FREELIST_OVERFLOW;

	if (freelist_internal_overlaps(freelist_internal_addr(alloc), sizeof(*alloc), original, size))
		return FREELIST_INVALID_ARGUMENT;

	size_t offset = (size_t)((FREE_LIST_ALIGNMENT - original % FREE_LIST_ALIGNMENT) % FREE_LIST_ALIGNMENT);
	if (offset > size) return FREELIST_INVALID_ARGUMENT;

	size_t available = (size - offset) & ~((size_t)FREE_LIST_ALIGNMENT - 1);
	if (available < FREE_LIST_MIN_ARENA_SIZE) return FREELIST_INVALID_ARGUMENT;

	block_t* head = (block_t*)((unsigned char*)memory + offset);

	freelist_internal_set_block(head, available - FREE_LIST_HEADER_SIZE, NULL, 1);

	alloc->head = head;
	alloc->arena_start = head;
	alloc->arena_size = available;
	alloc->magic = FREE_LIST_ALLOCATOR_MAGIC;

	return FREELIST_OK;
}

freelist_result freelist_alloc(freelist_allocator* alloc, size_t size, void** out) {
	freelist_result result = freelist_internal_check(alloc);
	if (result != FREELIST_OK) return result;

	result = freelist_internal_check_output(alloc, out);
	if (result != FREELIST_OK) return result;

	return freelist_internal_allocate(alloc, size, out);
}

freelist_result freelist_dealloc(freelist_allocator* alloc, void* ptr) {
	freelist_result result = freelist_internal_check(alloc);
	if (result != FREELIST_OK) return result;

	if (!ptr) return FREELIST_OK;

	block_t* block;
	result = freelist_internal_find_block(alloc, ptr, &block);

	if (result != FREELIST_OK) return result;

	freelist_internal_release(alloc, block);

	return FREELIST_OK;
}

freelist_result freelist_calloc(freelist_allocator* alloc, size_t count, size_t size, void** out) {
	freelist_result result = freelist_internal_check(alloc);
	if (result != FREELIST_OK) return result;

	result = freelist_internal_check_output(alloc, out);

	if (result != FREELIST_OK) return result;
	if (size && count > SIZE_MAX / size) return FREELIST_OVERFLOW;

	size_t bytes = count * size;
	void* memory;

	result = freelist_internal_allocate(alloc, bytes, &memory);
	if (result != FREELIST_OK) return result;

	for (size_t i = 0; i < bytes; i++) ((unsigned char*)memory)[i] = 0;

	*out = memory;

	return FREELIST_OK;
}

freelist_result freelist_realloc(freelist_allocator* alloc, void* ptr, size_t size, void** out) {
	freelist_result result = freelist_internal_check(alloc);
	if (result != FREELIST_OK) return result;

	result = freelist_internal_check_output(alloc, out);
	if (result != FREELIST_OK) return result;

	if (!ptr) return freelist_internal_allocate(alloc, size, out);

	block_t* block;
	result = freelist_internal_find_block(alloc, ptr, &block);

	if (result != FREELIST_OK) return result;
	if (size == 0) {
		freelist_internal_release(alloc, block);
		*out = NULL;
		return FREELIST_OK;
	}

	size_t requested;
	result = freelist_internal_request_size(size, &requested);

	if (result != FREELIST_OK) return result;
	if (requested <= block->size) {
		freelist_internal_trim(alloc, block, requested);
		*out = ptr;
		return FREELIST_OK;
	}

	uintptr_t next_address = freelist_internal_addr(block) + FREE_LIST_HEADER_SIZE + block->size;
	uintptr_t end = freelist_internal_addr(alloc->arena_start) + alloc->arena_size;

	if (next_address < end) {
		block_t* next = (block_t*)next_address;
		result = freelist_internal_check_block(alloc, next);

		if (result != FREELIST_OK) return result;
		if (next->free) {
			block_t** link = &alloc->head;

			while (*link && freelist_internal_addr(*link) < next_address) link = &(*link)->next_free;

			if (*link != next) return FREELIST_CORRUPTED;

			size_t combined = block->size + FREE_LIST_HEADER_SIZE + next->size;

			if (combined >= requested) {
				*link = next->next_free;
				block->size = combined;
				next->magic = 0;

				freelist_internal_trim(alloc, block, requested);
				*out = ptr;
				return FREELIST_OK;
			}
		}
	}

	block_t* replacement = freelist_internal_take(alloc, requested);
	if (!replacement) return FREELIST_OUT_OF_MEMORY;

	unsigned char* memory = (unsigned char*)replacement + FREE_LIST_HEADER_SIZE;

	for (size_t i = 0; i < block->size; i++) memory[i] = ((unsigned char*)ptr)[i];

	freelist_internal_release(alloc, block);

	*out = memory;
	return FREELIST_OK;
}

void* freelist_container_allocate(void* context, size_t bytes) {
	void* memory = NULL;
	if (freelist_alloc((freelist_allocator*)context, bytes, &memory) != FREELIST_OK) return NULL;
	return memory;
}

void freelist_container_deallocate(void* context, void* memory) {
	(void)freelist_dealloc((freelist_allocator*)context, memory);
}

#endif // FREE_LIST_IMPLEMENTATION_INCLUDED
#endif // FREE_LIST_IMPLEMENTATION
