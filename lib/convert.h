#ifndef RASPPIOS_CONVERT_H
#define RASPPIOS_CONVERT_H

#include <limits.h>
#include <stddef.h>
#include <stdint.h>

// Enough room for every supported integer/pointer, a sign/prefix, and '\0'.
#define CONVERT_BUFFER_SIZE (sizeof(uintmax_t) * CHAR_BIT + 3)

// Return 0 on success, 1 if capacity is too small (including the trailing '\0').
// On failure, str is empty if capacity > 0. str must be valid if capacity > 0.
int itos(intmax_t value, char* str, size_t capacity);
int utos(uintmax_t value, char* str, size_t capacity);
int utos_hex(uintmax_t value, char* str, size_t capacity);
int ptos_hex(const void* value, char* str, size_t capacity);

#endif
