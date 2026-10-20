#ifndef STRING_BUILDER_H
#define STRING_BUILDER_H

/* Define STRING_BUILDER_IMPLEMENTATION in exactly 1 file.
 * Define STRING_BUILDER_NO_STDLIB in all files to omit hosted-library use,
 * including the malloc/free adapter and formatted append.
 * C99 or later; <stddef.h> is a freestanding header.
 */

#include <stddef.h>

#ifndef CONTAINER_ALLOCATOR
#define CONTAINER_ALLOCATOR
typedef struct container_allocator {
	void *(*allocate)(void* context, size_t bytes); // Return suitably aligned storage or NULL on failure
	void (*deallocate)(void* context, void* memory);
	void* context;
} container_allocator;
#endif

/* Sizes and indices count bytes; size and capacity exclude the trailing '\0'.
 * Allocated buffers have capacity + 1 bytes and data[size] == '\0'.
 * Embedded nulls are supported. Zero initialization creates an empty builder.
 * Fields are for inspection; copying the struct does not duplicate ownership.
 */

typedef struct {
	size_t size, capacity;
	container_allocator allocator;
	char* data;
} string_builder;

/* Fallible operations return 0 on success, 1 on failure; failure changes nothing.
 * Growth may invalidate ALL buffer pointers. Append/insert grow geometrically.
 * Inputs must not overlap the builder or its buffer.
 * Unless noted, sb must be non-NULL and initialized (zero initialization is valid).
 * Without an allocator, growth uses malloc/free or fails with NO_STDLIB.
 */

/* Initialize without allocating; sb may be uninitialized but must not own a buffer.
 * The allocator is copied; both callbacks are required. Its context must outlive
 * the builder's allocations, including detached buffers.
 * NULL selects malloc/free or fails if STRING_BUILDER_NO_STDLIB is defined.
 */
int sb_init(string_builder* sb, const container_allocator* allocator);

/* Release the buffer and reset to the zero-initialized state. Accepts NULL. */
void sb_destroy(string_builder* sb);

/* Empty the string, retaining capacity and allocator. Accepts NULL. */
void sb_clear(string_builder* sb);

/* Ensure room for capacity content bytes; preserve size and never shrink. */
int sb_reserve(string_builder* sb, size_t capacity);

/* Append one byte, including '\0'. */
int sb_append_char(string_builder* sb, char c);

/* Append a non-NULL, null-terminated string, excluding its terminator. */
int sb_append(string_builder* sb, const char* str);

/* Append len bytes. str may be NULL only when len is 0. */
int sb_append_slice(string_builder* sb, const char* str, size_t len);

#ifndef STRING_BUILDER_NO_STDLIB
/* Append printf-style output using vsnprintf; formatting errors return 1.
 * fmt must be non-NULL; %n and arguments pointing into the builder are unsupported.
 * Nonempty output replaces the buffer on success, invalidating all buffer pointers.
 */
int sb_append_format(string_builder* sb, const char* fmt, ...);
#endif

/* Insert a non-NULL, null-terminated string, excluding its terminator.
 * index may equal size to append; index > size fails.
 */
int sb_insert(string_builder* sb, size_t index, const char* str);

/* Erase len bytes, preserving order and capacity.
 * Fail if index > size or len > size - index; len == 0 is a no-op at a valid index.
 */
int sb_erase(string_builder* sb, size_t index, size_t len);

/* Return a borrowed C string, or "" if no buffer exists. Do not free or modify it.
 * C string functions stop at the first embedded null; size counts all content.
 */
const char* sb_cstr(const string_builder* sb);

/* Detach the buffer without copying; return NULL if none exists.
 * Reset data/size/capacity, retaining the allocator. Save size first if needed.
 * Optionally write its allocator (zeroed for NULL); output must not alias sb/data.
 * Release the result with the matching deallocate(context, result), not necessarily
 * free(). If allocator is NULL, the caller must already know how to release it.
 */
char* sb_take(string_builder* sb, container_allocator* allocator);

/* Return 1 when size is 0, otherwise 0. */
int sb_is_empty(const string_builder* sb);

#endif

#ifdef STRING_BUILDER_IMPLEMENTATION
#ifndef STRING_BUILDER_IMPLEMENTATION_INCLUDED
#define STRING_BUILDER_IMPLEMENTATION_INCLUDED

#include <stdbool.h>

#ifndef STRING_BUILDER_NO_STDLIB
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#endif

#ifdef STRING_BUILDER_NO_STDLIB

static size_t _sb_strlen(const char* s) {
	size_t len = 0;
	while (*s++) len++;
	return len;
}

#endif

static size_t _sb_max_capacity(void) {
	return ((size_t)-1 - 1) / sizeof(char);
}

#ifndef STRING_BUILDER_NO_STDLIB

static void* _sb_allocate(void* context, size_t bytes) {
	(void)context;
	return malloc(bytes);
}

static void _sb_deallocate(void* context, void* memory) {
	(void)context;
	free(memory);
}

#endif

int sb_init(string_builder* sb, const container_allocator* allocator) {
	container_allocator selected;

	if (allocator) selected = *allocator;
	else {
		#ifdef STRING_BUILDER_NO_STDLIB
		return 1;
		#else
		selected.allocate = _sb_allocate;
		selected.deallocate = _sb_deallocate;
		selected.context = NULL;
		#endif
	}

	if (!selected.allocate || !selected.deallocate) return 1;

	sb->allocator = selected;
	sb->capacity = 0;
	sb->size = 0;
	sb->data = NULL;
	return 0;
}

void sb_destroy(string_builder* sb) {
	if (!sb) return;

	if (sb->data) sb->allocator.deallocate(sb->allocator.context, sb->data);
	sb->data = NULL;
	sb->size = 0;
	sb->capacity = 0;
	sb->allocator.allocate = NULL;
	sb->allocator.deallocate = NULL;
	sb->allocator.context = NULL;
}

void sb_clear(string_builder* sb) {
	if (!sb) return;

	if (!sb->data) return;

	sb->size = 0;
	sb->data[0] = 0;
}

int sb_reserve(string_builder* sb, size_t capacity) {
	if (!sb) return 1;

	if (capacity > _sb_max_capacity()) return 1;
	if (capacity <= sb->capacity) return 0;

	bool allocator_set = false;
	if (!sb->allocator.allocate || !sb->allocator.deallocate) {
		#ifdef STRING_BUILDER_NO_STDLIB
		if (!sb->allocator.allocate || !sb->allocator.deallocate) return 1;
		#else
		allocator_set = true;
		sb->allocator.allocate = _sb_allocate;
		sb->allocator.deallocate = _sb_deallocate;
		sb->allocator.context = NULL;
		#endif
	}

	if (!sb->data) {
		sb->data = (char*)sb->allocator.allocate(sb->allocator.context, capacity + 1);
		if (!sb->data) {
			if (allocator_set) sb->allocator = (container_allocator){0};
			return 1;
		}

		sb->capacity = capacity;
		sb->data[0] = 0;
		return 0;
	}

	char* new_data = (char*)sb->allocator.allocate(sb->allocator.context, capacity + 1);
	if (!new_data) {
		if (allocator_set) sb->allocator = (container_allocator){0};
		return 1;
	}

	for (size_t i = 0; i < sb->size; i++) new_data[i] = sb->data[i];
	sb->allocator.deallocate(sb->allocator.context, sb->data);
	sb->capacity = capacity;
	sb->data = new_data;
	sb->data[sb->size] = 0;
	return 0;
}

int sb_append_char(string_builder* sb, char c) {
	if (!sb) return 1;
	
	size_t capacity = sb->capacity;
	size_t maximum = _sb_max_capacity();

	if (sb->size == maximum) return 1;
	if (sb->size == capacity) {
		if (!capacity) capacity = 8;
		else capacity = capacity > maximum / 2 ? maximum : capacity * 2;
		if (sb_reserve(sb, capacity)) return 1;
	}

	sb->data[sb->size++] = c;
	sb->data[sb->size] = 0;
	return 0;
}

int sb_append(string_builder* sb, const char* str) {
	if (!sb) return 1;
	
	size_t capacity = sb->capacity;
	#ifdef STRING_BUILDER_NO_STDLIB
	size_t str_len = _sb_strlen(str);
	#else
	size_t str_len = strlen(str);
	#endif
	size_t maximum = _sb_max_capacity();
	
	if (str_len == 0) return 0;

	if (str_len > maximum - sb->size) return 1;
	if (sb->size + str_len > capacity) {
		size_t required = sb->size + str_len;
		capacity = capacity > maximum / 2 ? maximum : capacity * 2;
		capacity = capacity > required ? capacity : required;
		if (sb_reserve(sb, capacity)) return 1;
	}
	
	for (size_t i = 0; i < str_len; i++) {
		sb->data[sb->size++] = str[i];
	}

	sb->data[sb->size] = 0;
	return 0;
}

int sb_append_slice(string_builder* sb, const char* str, size_t len) {
	if (!sb) return 1;
	
	size_t capacity = sb->capacity;
	size_t maximum = _sb_max_capacity();

	if (len == 0) return 0;

	if (len > maximum - sb->size) return 1;
	if (sb->size + len > capacity) {
		size_t required = sb->size + len;
		capacity = capacity > maximum / 2 ? maximum : capacity * 2;
		capacity = capacity > required ? capacity : required;
		if (sb_reserve(sb, capacity)) return 1;
	}
	
	for (size_t i = 0; i < len; i++) {
		sb->data[sb->size++] = str[i];
	}

	sb->data[sb->size] = 0;
	return 0;
}

#ifndef STRING_BUILDER_NO_STDLIB

int sb_append_format(string_builder* sb, const char* fmt, ...) {
	if (!sb || !fmt) return 1;
	
	va_list args;
	va_start(args, fmt);

	va_list args_copy;
	va_copy(args_copy, args);

	size_t capacity = sb->capacity;
	size_t maximum = _sb_max_capacity();
	int needed = vsnprintf(NULL, 0, fmt, args_copy);
	va_end(args_copy);

	if (needed <= 0) {
		va_end(args);
		return needed < 0 ? 1 : 0;
	}

	if ((size_t)needed > maximum - sb->size) {
		va_end(args);
		return 1;
	}

	container_allocator selected = sb->allocator;
	if (!selected.allocate || !selected.deallocate) {
		selected.allocate = _sb_allocate;
		selected.deallocate = _sb_deallocate;
		selected.context = NULL;
	}

	size_t required = sb->size + (size_t)needed;
	if (required > capacity) {
		capacity = capacity > maximum / 2 ? maximum : capacity * 2;
		capacity = capacity > required ? capacity : required;
	}

	char* new_data = (char*)selected.allocate(selected.context, capacity + 1);
	if (!new_data) {
		va_end(args);
		return 1;
	}
	for (size_t i = 0; i < sb->size; i++) new_data[i] = sb->data[i];

	int written = vsnprintf(new_data + sb->size, capacity - sb->size + 1, fmt, args);
	va_end(args);

	if (written != needed) {
		selected.deallocate(selected.context, new_data);
		return 1;
	}

	if (sb->data) sb->allocator.deallocate(sb->allocator.context, sb->data);
	sb->data = new_data;
	sb->size = required;
	sb->capacity = capacity;
	sb->allocator = selected;

	return 0;
}

#endif

int sb_insert(string_builder* sb, size_t index, const char* str) {
	if (!sb) return 1;

	if (sb->size < index) return 1;
	
	size_t capacity = sb->capacity;
	#ifdef STRING_BUILDER_NO_STDLIB
	size_t str_len = _sb_strlen(str);
	#else
	size_t str_len = strlen(str);
	#endif
	size_t maximum = _sb_max_capacity();
	size_t size = sb->size;

	if (str_len == 0) return 0;

	if (str_len > maximum - size) return 1;
	if (size + str_len > capacity) {
		size_t required = size + str_len;
		capacity = capacity > maximum / 2 ? maximum : capacity * 2;
		capacity = capacity > required ? capacity : required;
		if (sb_reserve(sb, capacity)) return 1;
	}
	
	if (size != 0) {
		for (size_t i = 0; i < size - index; i++) {
			sb->data[size + str_len - i - 1] = sb->data[size - i - 1];
		}
	}
	for (size_t i = 0; i < str_len; i++) {
		sb->data[index + i] = str[i];
	}

	sb->size += str_len;
	sb->data[sb->size] = 0;
	return 0;
}

int sb_erase(string_builder* sb, size_t index, size_t len) {
	if (!sb) return 1;

	if (index > sb->size || len > sb->size - index) return 1;
	if (len == 0) return 0;
	
	for (size_t i = 0; i < sb->size - index - len; i++) {
		sb->data[index + i] = sb->data[index + len + i];
	}

	sb->size -= len;
	sb->data[sb->size] = 0;
	return 0;
}

const char* sb_cstr(const string_builder* sb) {
	if (!sb || !sb->data) return "";
	return sb->data;
}

char* sb_take(string_builder* sb, container_allocator* allocator) {
	if (!sb || !sb->data) {
		if (allocator) *allocator = (container_allocator){0};
		return NULL;
	}
	if (allocator) *allocator = sb->allocator;
	sb->capacity = 0;
	sb->size = 0;
	char* data = sb->data;
	sb->data = NULL;
	return data;
}

int sb_is_empty(const string_builder* sb) {
	if (!sb) return 0;
	if (sb->size == 0) return 1;
	return 0;
}

#endif
#endif
