#ifndef DYNAMIC_ARRAY_H
#define DYNAMIC_ARRAY_H
/* Invoke DYNAMIC_ARRAY_DECLARE(PREFIX, TYPE) at file scope, without a semicolon.
 * Put declarations in a guarded header when shared by multiple source files.
 * DYNAMIC_ARRAY_DEFINE(PREFIX, TYPE) follows matching declarations in exactly
 * 1 translation unit per prefix. Both macros may be used directly in a .c file.
 * Public functions use PREFIX_* and the iterator type is PREFIX_iter. Choose
 * a distinct prefix for each container/type instantiation across the program.
 * TYPE must be complete and assignable; use typedefs for pointers or comma-containing types.
 * Define DYNAMIC_ARRAY_NO_STDLIB before inclusion to omit the malloc/free adapter.
 * C99 or later.
 *
 * Elements are copied by value; referenced resources are not duplicated or freed.
 * Elements must tolerate relocation. Copying an owning struct does not clone it.
 * Metadata fields are for inspection; modify them through the API.
 * Zero initialization permits clear/destroy; call init before other operations.
 *
 * Fallible operations return 0 on success, 1 on failure; failure leaves the
 * container and outputs unchanged. Predicates return 1 for true, 0 for false.
 * Unless noted, container pointers must be non-NULL and initialized.
 * Growth may invalidate all element pointers. Insert/remove shift elements,
 * so a pointer may refer to a different value even without reallocation.
 */

#include <stddef.h>
#ifndef DYNAMIC_ARRAY_NO_STDLIB
#include <stdlib.h>
#endif         // DYNAMIC_ARRAY_NO_STDLIB

#ifndef CONTAINER_ALLOCATOR
#define CONTAINER_ALLOCATOR

typedef struct container_allocator {
	/* Return storage aligned for the requested types, or NULL on failure. */
	void* (*allocate)(void* context, size_t bytes);
	void (*deallocate)(void* context, void* memory);
	void* context;
} container_allocator;

#endif         // CONTAINER_ALLOCATOR

#define DYNAMIC_ARRAY_DECLARE(PREFIX, TYPE) DYNAMIC_ARRAY_INTERNAL_DECLARE(PREFIX, TYPE)
#define DYNAMIC_ARRAY_DEFINE(PREFIX, TYPE) DYNAMIC_ARRAY_INTERNAL_DEFINE(PREFIX, TYPE)

#define DYNAMIC_ARRAY_INTERNAL_DECLARE(PREFIX, TYPE) \
typedef struct PREFIX##_dynamic_array {                                                          \
	size_t size, capacity;                                                                   \
	container_allocator allocator;                                                           \
	TYPE* data;                                                                              \
} PREFIX##_dynamic_array;                                                                        \
                                                                                                 \
typedef struct PREFIX##_iter {                                                                   \
	const PREFIX##_dynamic_array* array;                                                     \
	size_t index;                                                                            \
} PREFIX##_iter;                                                                                 \
                                                                                                 \
/* Initialize without allocating; the destination may be uninitialized but must
 * not own storage. Both allocator callbacks are required; the allocator is copied
 * and its context must outlive the array's allocations. NULL selects malloc/free,
 * or fails if DYNAMIC_ARRAY_NO_STDLIB is defined.
 */                                                                                              \
int PREFIX##_init(PREFIX##_dynamic_array* darray, const container_allocator* allocator);         \
                                                                                                 \
/* Free backing storage and reset the object, not the caller-owned struct itself.
 * Accepts NULL and zero-initialized objects.
 */                                                                                              \
void PREFIX##_destroy(PREFIX##_dynamic_array* darray);                                           \
                                                                                                 \
/* Retain capacity and allocator. Accepts NULL and zero-initialized objects. */                  \
void PREFIX##_clear(PREFIX##_dynamic_array* darray);                                             \
                                                                                                 \
size_t PREFIX##_size(const PREFIX##_dynamic_array* darray);                                      \
size_t PREFIX##_capacity(const PREFIX##_dynamic_array* darray);                                  \
int PREFIX##_is_empty(const PREFIX##_dynamic_array* darray);                                     \
                                                                                                 \
/* Borrowed storage, or NULL if none is allocated. Only [0, size) holds elements. */             \
TYPE* PREFIX##_data(PREFIX##_dynamic_array* darray);                                             \
const TYPE* PREFIX##_const_data(const PREFIX##_dynamic_array* darray);                           \
                                                                                                 \
/* Return NULL for index >= size. Returned pointers are borrowed. */                             \
TYPE* PREFIX##_at(PREFIX##_dynamic_array* darray, size_t index);                                 \
const TYPE* PREFIX##_const_at(const PREFIX##_dynamic_array* darray, size_t index);               \
                                                                                                 \
/* Ensure capacity for at least this many elements; preserve size and never shrink.
 * Requests within current capacity succeed without allocating.
 */                                                                                              \
int PREFIX##_reserve(PREFIX##_dynamic_array* darray, size_t capacity);                           \
                                                                                                 \
int PREFIX##_push_back(PREFIX##_dynamic_array* darray, TYPE value);                              \
                                                                                                 \
/* Fail when empty. out may be NULL to discard the value; retain capacity. */                    \
int PREFIX##_pop_back(PREFIX##_dynamic_array* darray, TYPE* out);                                \
                                                                                                 \
/* index may equal size to append; index > size fails. */                                        \
int PREFIX##_insert(PREFIX##_dynamic_array* darray, size_t index, TYPE value);                   \
                                                                                                 \
/* Preserve order and capacity; index >= size fails. out may be NULL to discard the value. */    \
int PREFIX##_remove(PREFIX##_dynamic_array* darray, size_t index, TYPE* out);                    \
                                                                                                 \
/* Initialize new elements with (TYPE){0}. Shrinking retains capacity and does
 * not clean up element-owned resources.
 */                                                                                              \
int PREFIX##_resize(PREFIX##_dynamic_array* darray, size_t size);                                \
                                                                                                 \
/* Read-only, allocation-free traversal in index order. Keep the container alive
 * and at the same address. Changes to size, positions, or storage invalidate
 * iterators; no-op operations do not. Iterator fields are implementation details.
 */                                                                                              \
PREFIX##_iter PREFIX##_iter_begin(const PREFIX##_dynamic_array* darray);                         \
                                                                                                 \
/* Construct an iterator after the last element; next returns 0, prev yields the last. */         \
PREFIX##_iter PREFIX##_iter_end(const PREFIX##_dynamic_array* darray);                           \
                                                                                                 \
/* Begin positions before the first result. next yields a borrowed pointer and
 * advances: 1 for an element, 0 for exhaustion (sets *out to NULL). Repeated calls
 * after exhaustion return 0. iter and out must be non-NULL; iter must be valid.
 */                                                                                              \
int PREFIX##_iter_next(PREFIX##_iter* iter, const TYPE** out);                                    \
                                                                                                 \
/* Move backward, then yield a borrowed pointer. Start at iter_end for reverse traversal.
 * Return 1 for an element, 0 at the beginning (sets *out to NULL); repeated calls
 * remain exhausted. Switching direction yields the same element again. iter and
 * out must be non-NULL; iter must be valid.
 */                                                                                              \
int PREFIX##_iter_prev(PREFIX##_iter* iter, const TYPE** out);
	
	#ifndef DYNAMIC_ARRAY_NO_STDLIB
	
	#define DYNAMIC_ARRAY_INTERNAL_ALLOCATOR_FUNCTIONS(PREFIX)                               \
	static void* PREFIX##_internal_allocate(void* context, size_t bytes) {            \
		(void)context;                                                                   \
		return malloc(bytes);                                                            \
	}                                                                                        \
                                                                                                 \
	static void PREFIX##_internal_deallocate(void* context, void* memory) {           \
		(void)context;                                                                   \
		free(memory);                                                                    \
	}
	
	#define DYNAMIC_ARRAY_INTERNAL_SELECT_DEFAULT_ALLOCATOR(PREFIX, ALLOCATOR)               \
	do {                                                                                     \
		(ALLOCATOR).allocate = PREFIX##_internal_allocate;                               \
		(ALLOCATOR).deallocate = PREFIX##_internal_deallocate;                           \
		(ALLOCATOR).context = NULL;                                                      \
	} while (0)
	
	#else
	
	#define DYNAMIC_ARRAY_INTERNAL_ALLOCATOR_FUNCTIONS(PREFIX)
	#define DYNAMIC_ARRAY_INTERNAL_SELECT_DEFAULT_ALLOCATOR(PREFIX, ALLOCATOR)               \
	do {                                                                                     \
		return 1;                                                                        \
	} while (0)
	
	#endif // DYNAMIC_ARRAY_INTERNAL_SELECT_DEFAULT_ALLOCATOR

#define DYNAMIC_ARRAY_INTERNAL_DEFINE(PREFIX, TYPE)                                              \
DYNAMIC_ARRAY_INTERNAL_ALLOCATOR_FUNCTIONS(PREFIX)                                               \
                                                                                                 \
int PREFIX##_init(PREFIX##_dynamic_array* darray, const container_allocator* allocator) {        \
	if (darray == NULL) return 1;                                                            \
                                                                                                 \
	container_allocator selected;                                                            \
	if (allocator) {                                                                         \
		selected.allocate = allocator->allocate;                                         \
		selected.deallocate = allocator->deallocate;                                     \
		selected.context = allocator->context;                                           \
	}                                                                                        \
	else {                                                                                   \
		DYNAMIC_ARRAY_INTERNAL_SELECT_DEFAULT_ALLOCATOR(PREFIX, selected);               \
	}                                                                                        \
                                                                                                 \
	if (!selected.allocate || !selected.deallocate) return 1;                                \
                                                                                                 \
	*darray = (PREFIX##_dynamic_array){0};                                                   \
	darray->allocator = selected;                                                            \
	return 0;                                                                                \
}                                                                                                \
                                                                                                 \
void PREFIX##_destroy(PREFIX##_dynamic_array* darray) {                                          \
	if (!darray) return;                                                                     \
	if (darray->data) darray->allocator.deallocate(darray->allocator.context, darray->data); \
	*darray = (PREFIX##_dynamic_array){0};                                                   \
}                                                                                                \
                                                                                                 \
void PREFIX##_clear(PREFIX##_dynamic_array* darray) {                                            \
	if (!darray) return;                                                                     \
	darray->size = 0;                                                                        \
}                                                                                                \
                                                                                                 \
size_t PREFIX##_size(const PREFIX##_dynamic_array* darray) {                                     \
	if (!darray) return 0;                                                                   \
	return darray->size;                                                                     \
}                                                                                                \
                                                                                                 \
size_t PREFIX##_capacity(const PREFIX##_dynamic_array* darray) {                                 \
	if (!darray) return 0;                                                                   \
	return darray->capacity;                                                                 \
}                                                                                                \
                                                                                                 \
int PREFIX##_is_empty(const PREFIX##_dynamic_array* darray) {                                    \
	if (!darray) return 1;                                                                   \
	return darray->size == 0 ? 1 : 0;                                                        \
}                                                                                                \
                                                                                                 \
TYPE* PREFIX##_data(PREFIX##_dynamic_array* darray) {                                            \
	if (!darray || !darray->data) return NULL;                                               \
	return darray->data;                                                                     \
}                                                                                                \
                                                                                                 \
const TYPE* PREFIX##_const_data(const PREFIX##_dynamic_array* darray) {                          \
	if (!darray || !darray->data) return NULL;                                               \
	return darray->data;                                                                     \
}                                                                                                \
                                                                                                 \
TYPE* PREFIX##_at(PREFIX##_dynamic_array* darray, size_t index) {                                \
	if (!darray || !darray->data) return NULL;                                               \
	if (index >= darray->size) return NULL;                                                  \
	return &darray->data[index];                                                             \
}                                                                                                \
                                                                                                 \
const TYPE* PREFIX##_const_at(const PREFIX##_dynamic_array* darray, size_t index) {              \
	if (!darray || !darray->data) return NULL;                                               \
	if (index >= darray->size) return NULL;                                                  \
	return &darray->data[index];                                                             \
}                                                                                                \
                                                                                                 \
int PREFIX##_reserve(PREFIX##_dynamic_array* darray, size_t capacity) {                          \
	if (!darray) return 1;                                                                   \
	if (capacity > (size_t)-1 / sizeof(TYPE)) return 1;                                      \
	if (capacity <= darray->capacity) return 0;                                              \
                                                                                                 \
	container_allocator* allocator = &darray->allocator;                                     \
	if (!allocator->allocate || !allocator->deallocate) return 1;                            \
                                                                                                 \
	TYPE* new_data = allocator->allocate(allocator->context, capacity * sizeof(TYPE));       \
	if (!new_data) return 1;                                                                 \
                                                                                                 \
	for (size_t i = 0; i < darray->size; i++) new_data[i] = darray->data[i];                 \
                                                                                                 \
	if (darray->data) allocator->deallocate(allocator->context, darray->data);               \
	darray->data = new_data;                                                                 \
	darray->capacity = capacity;                                                             \
	return 0;                                                                                \
}                                                                                                \
                                                                                                 \
int PREFIX##_push_back(PREFIX##_dynamic_array* darray, TYPE value) {                             \
	if (!darray) return 1;                                                                   \
                                                                                                 \
	size_t size = darray->size;                                                              \
	size_t capacity = darray->capacity;                                                      \
	size_t maximum = (size_t)-1 / sizeof(TYPE);                                              \
                                                                                                 \
	if (size == maximum) return 1;                                                           \
	if (size == capacity) {                                                                  \
		if (!capacity) capacity = maximum < 8 ? maximum : 8;                             \
		else capacity = capacity > maximum / 2 ? maximum : capacity * 2;                 \
		if (PREFIX##_reserve(darray, capacity)) return 1;                                \
	}                                                                                        \
                                                                                                 \
	darray->data[darray->size++] = value;                                                    \
	return 0;                                                                                \
}                                                                                                \
                                                                                                 \
int PREFIX##_pop_back(PREFIX##_dynamic_array* darray, TYPE* out) {                               \
	if (!darray) return 1;                                                                   \
	if (darray->size == 0) return 1;                                                         \
                                                                                                 \
	darray->size--;                                                                          \
	if (out) *out = darray->data[darray->size];                                              \
	return 0;                                                                                \
}                                                                                                \
                                                                                                 \
int PREFIX##_insert(PREFIX##_dynamic_array* darray, size_t index, TYPE value) {                  \
	if (!darray) return 1;                                                                   \
	if (index > darray->size) return 1;                                                      \
	if (PREFIX##_push_back(darray, value)) return 1;                                         \
                                                                                                 \
	for (size_t i = darray->size - 1; i > index; i--) darray->data[i] = darray->data[i - 1]; \
	darray->data[index] = value;                                                             \
	return 0;                                                                                \
}                                                                                                \
                                                                                                 \
int PREFIX##_remove(PREFIX##_dynamic_array* darray, size_t index, TYPE* out) {                   \
	if (!darray) return 1;                                                                   \
	if (index >= darray->size) return 1;                                                     \
                                                                                                 \
	if (out) *out = darray->data[index];                                                     \
	for (size_t i = index; i < darray->size - 1; i++) darray->data[i] = darray->data[i + 1]; \
	darray->size--;                                                                          \
	return 0;                                                                                \
}                                                                                                \
                                                                                                 \
int PREFIX##_resize(PREFIX##_dynamic_array* darray, size_t size) {                               \
	if (!darray) return 1;                                                                   \
                                                                                                 \
	if (size <= darray->size) {                                                              \
		darray->size = size;                                                             \
		return 0;                                                                        \
	}                                                                                        \
	else if(PREFIX##_reserve(darray, size)) return 1;                                        \
                                                                                                 \
	for (size_t i = darray->size; i < size; i++) darray->data[i] = (TYPE){0};                \
	darray->size = size;                                                                     \
	return 0;                                                                                \
}                                                                                                \
                                                                                                 \
PREFIX##_iter PREFIX##_iter_begin(const PREFIX##_dynamic_array* darray) {                        \
	if (!darray) return (PREFIX##_iter){NULL, 0};                                            \
	return (PREFIX##_iter){darray, 0};                                                       \
}                                                                                                \
                                                                                                 \
PREFIX##_iter PREFIX##_iter_end(const PREFIX##_dynamic_array* darray) {                          \
	if (!darray) return (PREFIX##_iter){NULL, 0};                                            \
	return (PREFIX##_iter){darray, darray->size};                                            \
}                                                                                                \
                                                                                                 \
int PREFIX##_iter_next(PREFIX##_iter* iter, const TYPE** out) {                                  \
	if (!iter || !out || !iter->array) return 0;                                             \
	if (iter->index >= iter->array->size) {                                                  \
		*out = NULL;                                                                     \
		return 0;                                                                        \
	}                                                                                        \
	*out = PREFIX##_const_at(iter->array, iter->index++);                                    \
	return 1;                                                                                \
}                                                                                                \
                                                                                                 \
int PREFIX##_iter_prev(PREFIX##_iter* iter, const TYPE** out) {                                  \
	if (!iter || !out || !iter->array) return 0;                                             \
	if (iter->index == 0) {                                                                  \
		*out = NULL;                                                                     \
		return 0;                                                                        \
	}                                                                                        \
	*out = PREFIX##_const_at(iter->array, --iter->index);                                    \
	return 1;                                                                                \
}

#endif         // DYNAMIC_ARRAY_H
