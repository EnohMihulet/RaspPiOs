#ifndef RASPPIOS_ALIGN_H
#define RASPPIOS_ALIGN_H

#include <stddef.h>
#include <stdint.h>

// Alignment must be a nonzero power of two.
// For align_up, the rounded result must be representable in the return type.

static inline uint8_t u8_align_down(uint8_t value, uint8_t alignment) {
	return value & ~(alignment - 1);
}

static inline uint8_t u8_align_up(uint8_t value, uint8_t alignment) {
	return (value + alignment - 1) & ~(alignment - 1);
}

static inline uint16_t u16_align_down(uint16_t value, uint16_t alignment) {
	return value & ~(alignment - 1);
}

static inline uint16_t u16_align_up(uint16_t value, uint16_t alignment) {
	return (value + alignment - 1) & ~(alignment - 1);
}

static inline uint32_t u32_align_down(uint32_t value, uint32_t alignment) {
	return value & ~(alignment - 1);
}

static inline uint32_t u32_align_up(uint32_t value, uint32_t alignment) {
	return (value + alignment - 1) & ~(alignment - 1);
}

static inline uint64_t u64_align_down(uint64_t value, uint64_t alignment) {
	return value & ~(alignment - 1);
}

static inline uint64_t u64_align_up(uint64_t value, uint64_t alignment) {
	return (value + alignment - 1) & ~(alignment - 1);
}

static inline size_t size_align_down(size_t value, size_t alignment) {
	return value & ~(alignment - 1);
}

static inline size_t size_align_up(size_t value, size_t alignment) {
	return (value + alignment - 1) & ~(alignment - 1);
}

static inline uintptr_t uintptr_align_down(uintptr_t value, uintptr_t alignment) {
	return value & ~(alignment - 1);
}

static inline uintptr_t uintptr_align_up(uintptr_t value, uintptr_t alignment) {
	return (value + alignment - 1) & ~(alignment - 1);
}

#endif
