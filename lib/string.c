#include <stddef.h>
#include <stdint.h>

#include "string.h"

size_t strlen(const char* s) {
	size_t len = 0;
	while (*s++) len++;
	return len;
}

int strcmp(const char* s1, const char* s2) {
	while (*s1 && *s1 == *s2) {
		s1++;
		s2++;
	}
	unsigned char a = (unsigned char)*s1;
	unsigned char b = (unsigned char)*s2;
	return (a > b) - (a < b);
}

void* memcpy(void* dst, const void* src, size_t n) {
	uint8_t* d = (uint8_t*)dst;
	const uint8_t* s = (const uint8_t*)src;
	while (n--) *d++ = *s++;
	return dst;
}

void* memset(void* s, int c, size_t n) {
	uint8_t* p = (uint8_t*)s;
	uint8_t value = (uint8_t)c;
	while (n--) *p++ = value;
	return s;
}

void* memmove(void* dst, const void* src, size_t n) {
	uint8_t* d = (uint8_t*)dst;
	const uint8_t* s = (const uint8_t*)src;

	if (d == s || n == 0) return dst;
	if ((uintptr_t)d < (uintptr_t)s) {
		while (n--) *d++ = *s++;
	}
	else {
		d += n;
		s += n;
		while (n--) {
			*--d = *--s;
		}
	}

	return dst;
}
