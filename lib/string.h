#ifndef RASPPIOS_STRING_H
#define RASPPIOS_STRING_H

#include <stddef.h>
#include <stdint.h>

size_t strlen(const char* s);

int strcmp(const char* s1, const char* s2);

void* memcpy(void* dst, const void* src, size_t n);

void* memset(void* s, int c, size_t n);

void* memmove(void* dst, const void* src, size_t n);

#endif
