#include "convert.h"

static int unsigned_to_string(uintmax_t value, char* str, size_t capacity,
		unsigned int base) {
	const char digits[] = "0123456789abcdef";
	size_t length = 1;
	uintmax_t remaining = value;

	if (capacity == 0) return 1;
	str[0] = '\0';
	while (remaining >= base) {
		remaining /= base;
		length++;
	}
	if (capacity <= length) return 1;

	str[length] = '\0';
	do {
		str[--length] = digits[value % base];
		value /= base;
	} while (length > 0);
	return 0;
}

int itos(intmax_t value, char* str, size_t capacity) {
	if (value >= 0) return utos((uintmax_t)value, str, capacity);
	if (capacity == 0) return 1;
	str[0] = '\0';
	if (capacity < 2) return 1;

	// Unsigned subtraction also handles INTMAX_MIN without signed overflow.
	uintmax_t magnitude = 0 - (uintmax_t)value;
	if (utos(magnitude, str + 1, capacity - 1)) return 1;
	str[0] = '-';
	return 0;
}

int utos(uintmax_t value, char* str, size_t capacity) {
	return unsigned_to_string(value, str, capacity, 10);
}

int utos_hex(uintmax_t value, char* str, size_t capacity) {
	return unsigned_to_string(value, str, capacity, 16);
}

int ptos_hex(const void* value, char* str, size_t capacity) {
	if (capacity == 0) return 1;
	str[0] = '\0';
	if (capacity < 4) return 1;
	if (utos_hex((uintptr_t)value, str + 2, capacity - 2)) return 1;
	str[0] = '0';
	str[1] = 'x';
	return 0;
}
