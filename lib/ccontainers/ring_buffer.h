#ifndef RING_BUFFER_H
#define RING_BUFFER_H

/* Invoke RING_BUFFER_DECLARE(PREFIX, TYPE) at file scope without a semicolon.
 * RING_BUFFER_DEFINE(PREFIX, TYPE) follows matching declarations in exactly
 * 1 translation unit per prefix. Both macros may be used directly in a .c file.
 * Public names use PREFIX_*; choose distinct prefixes across containers/types.
 * TYPE must be complete and assignable; use typedefs for pointers or types with commas.
 * Define RING_BUFFER_NO_STDLIB before inclusion to omit the malloc/free adapter.
 * C99 or later.
 *
 * Elements are shallow value copies; referenced resources are not duplicated or
 * freed. Elements must tolerate relocation. Copying an owning struct does not
 * clone it. Metadata fields are for inspection; modify them through the API.
 * Zero initialization permits clear/destroy; call init before other operations.
 * Unless noted, container pointers must be non-NULL and initialized.
 *
 * Fallible operations return 0 on success, 1 on failure; failure leaves the
 * container and outputs unchanged. Predicates return 1 for true, 0 for false.
 * Indices count logical elements from the front, not physical storage slots.
 * Full buffers grow automatically; insertion never overwrites existing elements.
 * Growth may invalidate all element pointers; insert/remove may shift elements.
 * For pop_front, pop_back, and remove, non-NULL out must point to writable TYPE
 * storage outside the ring's backing allocation.
 */

#include <stddef.h>
#ifndef RING_BUFFER_NO_STDLIB
#include <stdlib.h>
#endif

#ifndef CONTAINER_ALLOCATOR
#define CONTAINER_ALLOCATOR

typedef struct container_allocator {
	/* Return storage aligned for the requested types, or NULL on failure. */
	void* (*allocate)(void* context, size_t bytes);
	void (*deallocate)(void* context, void* memory);
	void* context;
} container_allocator;

#endif // CONTAINER_ALLOCATOR

#define RING_BUFFER_DECLARE(PREFIX, TYPE) RING_BUFFER_INTERNAL_DECLARE(PREFIX, TYPE)
#define RING_BUFFER_DEFINE(PREFIX, TYPE) RING_BUFFER_INTERNAL_DEFINE(PREFIX, TYPE)

#define RING_BUFFER_INTERNAL_DECLARE(PREFIX, TYPE)                                                                           \
typedef struct PREFIX##_ring_buffer {                                                                                        \
	size_t size, capacity;                                                                                               \
	/* Physical occupied endpoints when nonempty; both are 0 when empty. */                                                \
	size_t front, back;                                                                                                  \
	container_allocator allocator;                                                                                       \
	TYPE* data;                                                                                                          \
} PREFIX##_ring_buffer;                                                                                                      \
                                                                                                                             \
typedef struct PREFIX##_iter {                                                                                               \
	const PREFIX##_ring_buffer* buf;                                                                                     \
	size_t index;                                                                                                        \
} PREFIX##_iter;                                                                                                             \
                                                                                                                             \
/* Initialize without allocating; the destination may be uninitialized but must
 * not own storage. Both allocator callbacks are required; the allocator is copied
 * and its context must outlive the buffer's allocations. NULL selects malloc/free,
 * or fails if RING_BUFFER_NO_STDLIB is defined.
 */                                                                                                                          \
int PREFIX##_init(PREFIX##_ring_buffer* rbuf, const container_allocator* allocator);                                         \
                                                                                                                             \
/* Free backing storage and reset the object, not the caller-owned struct itself.
 * Accepts NULL and zero-initialized objects.
 */                                                                                                                          \
void PREFIX##_destroy(PREFIX##_ring_buffer* rbuf);                                                                           \
                                                                                                                             \
/* Retain capacity and allocator. Accepts NULL and zero-initialized objects. */                                              \
void PREFIX##_clear(PREFIX##_ring_buffer* rbuf);                                                                             \
                                                                                                                             \
size_t PREFIX##_size(const PREFIX##_ring_buffer* rbuf);                                                                      \
size_t PREFIX##_capacity(const PREFIX##_ring_buffer* rbuf);                                                                  \
int PREFIX##_is_empty(const PREFIX##_ring_buffer* rbuf);                                                                     \
                                                                                                                             \
/* Return NULL for index >= size. Returned pointers are borrowed. */                                                         \
TYPE* PREFIX##_at(PREFIX##_ring_buffer* rbuf, size_t index);                                                                 \
const TYPE* PREFIX##_const_at(const PREFIX##_ring_buffer* rbuf, size_t index);                                               \
                                                                                                                             \
/* Borrowed endpoint pointers, or NULL when empty. */                                                                          \
TYPE* PREFIX##_front(PREFIX##_ring_buffer* rbuf);                                                                             \
const TYPE* PREFIX##_const_front(const PREFIX##_ring_buffer* rbuf);                                                         \
TYPE* PREFIX##_back(PREFIX##_ring_buffer* rbuf);                                                                              \
const TYPE* PREFIX##_const_back(const PREFIX##_ring_buffer* rbuf);                                                          \
                                                                                                                             \
/* Ensure capacity for at least this many elements; preserve size and never shrink.
 * Requests within current capacity succeed without allocating.
 */                                                                                                                          \
int PREFIX##_reserve(PREFIX##_ring_buffer* rbuf, size_t capacity);                                                           \
                                                                                                                             \
int PREFIX##_push_back(PREFIX##_ring_buffer* rbuf, TYPE value);                                                              \
                                                                                                                             \
/* Fail when empty. out may be NULL to discard the value; retain capacity. */                                                \
int PREFIX##_pop_back(PREFIX##_ring_buffer* rbuf, TYPE* out);                                                                \
                                                                                                                             \
int PREFIX##_push_front(PREFIX##_ring_buffer* rbuf, TYPE value);                                                             \
                                                                                                                             \
/* Fail when empty. out may be NULL to discard the value; retain capacity. */                                                \
int PREFIX##_pop_front(PREFIX##_ring_buffer* rbuf, TYPE* out);                                                               \
                                                                                                                             \
/* index may equal size to append; index > size fails. */                                                                    \
int PREFIX##_insert(PREFIX##_ring_buffer* rbuf, size_t index, TYPE value);                                                   \
                                                                                                                             \
/* Preserve order and capacity; index >= size fails. out may be NULL to discard the value. */                                \
int PREFIX##_remove(PREFIX##_ring_buffer* rbuf, size_t index, TYPE* out);                                                    \
                                                                                                                             \
/* Grow by appending (TYPE){0} elements at the back; shrink by removing the suffix.
 * Retain capacity when shrinking; element-owned resources are not cleaned up.
 */                                                                                                                          \
int PREFIX##_resize(PREFIX##_ring_buffer* rbuf, size_t size);                                                                \
                                                                                                                             \
/* Read-only, allocation-free traversal in logical front-to-back order. Keep the container alive
 * and at the same address. Changes to size, positions, or storage invalidate
 * iterators; no-op operations do not. Iterator fields are implementation details.
 */                                                                                                                          \
PREFIX##_iter PREFIX##_iter_begin(const PREFIX##_ring_buffer* rbuf);                                                         \
                                                                                                                             \
/* Construct an iterator after the last element; next returns 0, prev yields the last. */                                    \
PREFIX##_iter PREFIX##_iter_end(const PREFIX##_ring_buffer* rbuf);                                                           \
                                                                                                                             \
/* Begin positions before the first result. next yields a borrowed pointer and
 * advances: 1 for an element, 0 for exhaustion (sets *out to NULL). Repeated calls
 * after exhaustion return 0. iter and out must be non-NULL; iter must be valid.
 */                                                                                                                          \
int PREFIX##_iter_next(PREFIX##_iter* iter, const TYPE** out);                                                               \
                                                                                                                             \
/* Move backward, then yield a borrowed pointer. Start at iter_end for reverse traversal.
 * Return 1 for an element, 0 at the beginning (sets *out to NULL); repeated calls
 * remain exhausted. Switching direction yields the same element again. iter and
 * out must be non-NULL; iter must be valid.
 */                                                                                                                         \
int PREFIX##_iter_prev(PREFIX##_iter* iter, const TYPE** out);                                                               \
	
	#ifndef RING_BUFFER_NO_STDLIB
	
	#define RING_BUFFER_INTERNAL_ALLOCATOR_FUNCTIONS(PREFIX)                                                             \
	static void* PREFIX##_internal_allocate(void* context, size_t bytes) {                                          \
		(void)context;                                                                                               \
		return malloc(bytes);                                                                                        \
	}                                                                                                                    \
                                                                                                                             \
	static void PREFIX##_internal_deallocate(void* context, void* memory) {                                         \
		(void)context;                                                                                               \
		free(memory);                                                                                                \
	}
	
	#define RING_BUFFER_INTERNAL_SELECT_DEFAULT_ALLOCATOR(PREFIX, ALLOCATOR)                                             \
	do {                                                                                                                 \
		(ALLOCATOR).allocate = PREFIX##_internal_allocate;                                                      \
		(ALLOCATOR).deallocate = PREFIX##_internal_deallocate;                                                  \
		(ALLOCATOR).context = NULL;                                                                                  \
	} while (0)
	
	#else
	
	#define RING_BUFFER_INTERNAL_ALLOCATOR_FUNCTIONS(PREFIX)
	#define RING_BUFFER_INTERNAL_SELECT_DEFAULT_ALLOCATOR(PREFIX, ALLOCATOR)                                             \
	do {                                                                                                                 \
		return 1;                                                                                                    \
	} while (0)
	
	#endif // RING_BUFFER_INTERNAL_SELECT_DEFAULT_ALLOCATOR

#define RING_BUFFER_INTERNAL_DEFINE(PREFIX, TYPE)                                                                            \
\
RING_BUFFER_INTERNAL_ALLOCATOR_FUNCTIONS(PREFIX) \
\
/* Logical to physical index; requires capacity > 0 and i < capacity. */                                                     \
static size_t PREFIX##_internal_index(const PREFIX##_ring_buffer* rbuf, size_t i) {                                         \
	size_t until_wrap = rbuf->capacity - rbuf->front;                                                                     \
	return i < until_wrap ? rbuf->front + i : i - until_wrap;                                                             \
}                                                                                                                            \
                                                                                                                             \
static size_t PREFIX##_internal_inc(const PREFIX##_ring_buffer* rbuf, size_t i) {                                                   \
	return (i + 1 == rbuf->capacity) ? 0 : i + 1;                                                                        \
}                                                                                                                            \
                                                                                                                             \
static size_t PREFIX##_internal_dec(const PREFIX##_ring_buffer* rbuf, size_t i) {                                                    \
	return (i == 0) ? rbuf->capacity - 1 : i - 1;                                                                        \
}                                                                                                                            \
                                                                                                                             \
static int PREFIX##_internal_grow(PREFIX##_ring_buffer* rbuf) {                                                              \
	if (!rbuf) return 1;                                                                                                 \
                                                                                                                             \
	size_t size = rbuf->size;                                                                                            \
	size_t capacity = rbuf->capacity;                                                                                    \
	size_t maximum = (size_t)-1 / sizeof(TYPE);                                                                          \
                                                                                                                             \
	if (size == maximum) return 1;                                                                                       \
	if (size == capacity) {                                                                                              \
		if (!capacity) capacity = maximum < 8 ? maximum : 8;                                                         \
		else capacity = capacity > maximum / 2 ? maximum : capacity * 2;                                             \
		if (PREFIX##_reserve(rbuf, capacity)) return 1;                                                              \
	}                                                                                                                    \
	return 0;                                                                                                            \
}                                                                                                                            \
                                                                                                                             \
int PREFIX##_init(PREFIX##_ring_buffer* rbuf, const container_allocator* allocator) {                                        \
	if (rbuf == NULL) return 1;                                                                                          \
                                                                                                                             \
	container_allocator selected;                                                                                        \
	if (allocator) {                                                                                                     \
		selected.allocate = allocator->allocate;                                                                     \
		selected.deallocate = allocator->deallocate;                                                                 \
		selected.context = allocator->context;                                                                       \
	}                                                                                                                    \
	else {                                                                                                               \
		RING_BUFFER_INTERNAL_SELECT_DEFAULT_ALLOCATOR(PREFIX, selected);                                             \
	}                                                                                                                    \
                                                                                                                             \
	if (!selected.allocate || !selected.deallocate) return 1;                                                            \
                                                                                                                             \
	*rbuf = (PREFIX##_ring_buffer){0};                                                                                    \
	rbuf->allocator = selected;                                                                                          \
	return 0;                                                                                                            \
}                                                                                                                            \
                                                                                                                             \
void PREFIX##_destroy(PREFIX##_ring_buffer* rbuf) {                                                                          \
	if (!rbuf) return;                                                                                                   \
	if (rbuf->data) rbuf->allocator.deallocate(rbuf->allocator.context, rbuf->data);                                     \
	*rbuf = (PREFIX##_ring_buffer){0};                                                                                   \
}                                                                                                                            \
                                                                                                                             \
void PREFIX##_clear(PREFIX##_ring_buffer* rbuf) {                                                                            \
	if (!rbuf) return;                                                                                                   \
	rbuf->size = 0;                                                                                                      \
	rbuf->front = 0;                                                                                                     \
	rbuf->back = 0;                                                                                                      \
}                                                                                                                            \
                                                                                                                             \
size_t PREFIX##_size(const PREFIX##_ring_buffer* rbuf) {                                                                     \
	if (!rbuf) return 0;                                                                                                 \
	return rbuf->size;                                                                                                   \
}                                                                                                                            \
                                                                                                                             \
size_t PREFIX##_capacity(const PREFIX##_ring_buffer* rbuf) {                                                                 \
	if (!rbuf) return 0;                                                                                                 \
	return rbuf->capacity;                                                                                               \
}                                                                                                                            \
                                                                                                                             \
int PREFIX##_is_empty(const PREFIX##_ring_buffer* rbuf) {                                                                    \
	if (!rbuf) return 1;                                                                                                 \
	return rbuf->size == 0 ? 1 : 0;                                                                                      \
}                                                                                                                            \
                                                                                                                             \
TYPE* PREFIX##_at(PREFIX##_ring_buffer* rbuf, size_t index) {                                                                \
	if (!rbuf || !rbuf->data) return NULL;                                                                               \
	if (index >= rbuf->size) return NULL;                                                                                \
	return &rbuf->data[PREFIX##_internal_index(rbuf, index)];                                                              \
}                                                                                                                            \
                                                                                                                             \
const TYPE* PREFIX##_const_at(const PREFIX##_ring_buffer* rbuf, size_t index) {                                              \
	if (!rbuf || !rbuf->data) return NULL;                                                                               \
	if (index >= rbuf->size) return NULL;                                                                                \
	return &rbuf->data[PREFIX##_internal_index(rbuf, index)];                                                              \
}                                                                                                                            \
                                                                                                                             \
TYPE* PREFIX##_front(PREFIX##_ring_buffer* rbuf) {                                                                           \
	if (!rbuf || !rbuf->data) return NULL;                                                                               \
	if (rbuf->size == 0) return NULL;                                                                                    \
	return &rbuf->data[rbuf->front];                                                                                     \
}                                                                                                                            \
                                                                                                                             \
const TYPE* PREFIX##_const_front(const PREFIX##_ring_buffer* rbuf) {                                                         \
	if (!rbuf || !rbuf->data) return NULL;                                                                               \
	if (rbuf->size == 0) return NULL;                                                                                    \
	return &rbuf->data[rbuf->front];                                                                                     \
}                                                                                                                            \
                                                                                                                             \
TYPE* PREFIX##_back(PREFIX##_ring_buffer* rbuf) {                                                                            \
	if (!rbuf || !rbuf->data) return NULL;                                                                               \
	if (rbuf->size == 0) return NULL;                                                                                    \
	return &rbuf->data[rbuf->back];                                                                                      \
}                                                                                                                            \
                                                                                                                             \
const TYPE* PREFIX##_const_back(const PREFIX##_ring_buffer* rbuf) {                                                          \
	if (!rbuf || !rbuf->data) return NULL;                                                                               \
	if (rbuf->size == 0) return NULL;                                                                                    \
	return &rbuf->data[rbuf->back];                                                                                      \
}                                                                                                                            \
                                                                                                                             \
int PREFIX##_reserve(PREFIX##_ring_buffer* rbuf, size_t capacity) {                                                          \
	if (!rbuf) return 1;                                                                                                 \
	if (capacity > (size_t)-1 / sizeof(TYPE)) return 1;                                                                  \
	if (capacity <= rbuf->capacity) return 0;                                                                            \
                                                                                                                             \
	container_allocator* allocator = &rbuf->allocator;                                                                   \
	if (!allocator->allocate || !allocator->deallocate) return 1;                                                        \
                                                                                                                             \
	TYPE* new_data = allocator->allocate(allocator->context, capacity * sizeof(TYPE));                                   \
	if (!new_data) return 1;                                                                                             \
                                                                                                                             \
	for (size_t i = 0; i < rbuf->size; i++) {                                                                            \
		new_data[i] = rbuf->data[PREFIX##_internal_index(rbuf, i)];                                                  \
	}                                                                                                                    \
                                                                                                                             \
	if (rbuf->data) allocator->deallocate(allocator->context, rbuf->data);                                               \
	rbuf->front = 0;                                                                                                     \
	rbuf->back = rbuf->size ? rbuf->size - 1 : 0;                                                                         \
	rbuf->data = new_data;                                                                                               \
	rbuf->capacity = capacity;                                                                                           \
	return 0;                                                                                                            \
}                                                                                                                            \
                                                                                                                             \
int PREFIX##_push_back(PREFIX##_ring_buffer* rbuf, TYPE value) {                                                             \
	if (!rbuf) return 1;                                                                                                 \
                                                                                                                             \
	if (PREFIX##_internal_grow(rbuf)) return 1;                                                                          \
                                                                                                                             \
	if (rbuf->size == 0) {                                                                                               \
		rbuf->front = 0;                                                                                             \
		rbuf->back = 0;                                                                                              \
	}                                                                                                                    \
	else {                                                                                                               \
		rbuf->back = PREFIX##_internal_inc(rbuf, rbuf->back);                                                        \
	}                                                                                                                    \
	rbuf->data[rbuf->back] = value;                                                                                       \
	rbuf->size++;                                                                                                        \
	return 0;                                                                                                            \
}                                                                                                                            \
                                                                                                                             \
int PREFIX##_pop_back(PREFIX##_ring_buffer* rbuf, TYPE* out) {                                                               \
	if (!rbuf) return 1;                                                                                                 \
                                                                                                                             \
	if (rbuf->size == 0) return 1;                                                                                       \
                                                                                                                             \
	if (out) *out = rbuf->data[rbuf->back];                                                                              \
	rbuf->size--;                                                                                                        \
                                                                                                                             \
	if (PREFIX##_is_empty(rbuf)) {                                                                                       \
		rbuf->front = 0;                                                                                             \
		rbuf->back = 0;                                                                                              \
	}                                                                                                                    \
	else {                                                                                                               \
		rbuf->back = PREFIX##_internal_dec(rbuf, rbuf->back);                                                        \
	}                                                                                                                    \
                                                                                                                             \
	return 0;                                                                                                            \
}                                                                                                                            \
                                                                                                                             \
int PREFIX##_push_front(PREFIX##_ring_buffer* rbuf, TYPE value) {                                                            \
	if (!rbuf) return 1;                                                                                                 \
                                                                                                                             \
	if (PREFIX##_internal_grow(rbuf)) return 1;                                                                          \
                                                                                                                             \
	if (rbuf->size == 0) {                                                                                               \
		rbuf->front = 0;                                                                                             \
		rbuf->back = 0;                                                                                              \
	}                                                                                                                    \
	else {                                                                                                               \
		rbuf->front = PREFIX##_internal_dec(rbuf, rbuf->front);                                                      \
	}                                                                                                                    \
                                                                                                                             \
	rbuf->data[rbuf->front] = value;                                                                                      \
	rbuf->size++;                                                                                                        \
	return 0;                                                                                                            \
}                                                                                                                            \
                                                                                                                             \
int PREFIX##_pop_front(PREFIX##_ring_buffer* rbuf, TYPE* out) {                                                              \
	if (!rbuf) return 1;                                                                                                 \
                                                                                                                             \
	if (rbuf->size == 0) return 1;                                                                                       \
                                                                                                                             \
	if (out) *out = rbuf->data[rbuf->front];                                                                             \
	rbuf->size--;                                                                                                        \
                                                                                                                             \
	if (PREFIX##_is_empty(rbuf)) {                                                                                       \
		rbuf->front = 0;                                                                                             \
		rbuf->back = 0;                                                                                              \
	}                                                                                                                    \
	else {                                                                                                               \
		rbuf->front = PREFIX##_internal_inc(rbuf, rbuf->front);                                                      \
	}                                                                                                                    \
                                                                                                                             \
	return 0;                                                                                                            \
}                                                                                                                            \
                                                                                                                             \
int PREFIX##_insert(PREFIX##_ring_buffer* rbuf, size_t index, TYPE value) {                                                  \
	if (!rbuf) return 1;                                                                                                 \
	if (index > rbuf->size) return 1;                                                                                    \
	if (rbuf->size == 0) return PREFIX##_push_back(rbuf, value);                                                          \
                                                                                                                             \
	if (PREFIX##_internal_grow(rbuf)) return 1;                                                                          \
                                                                                                                             \
	size_t left = index;                                                                                                 \
	size_t right = rbuf->size - index;                                                                                   \
	if (left < right) {                                                                                                  \
		for (size_t i = 0; i < index; i++) {                                                                         \
			size_t source = PREFIX##_internal_index(rbuf, i);                                                          \
			rbuf->data[PREFIX##_internal_dec(rbuf, source)] = rbuf->data[source];                                       \
		}                                                                                                            \
		rbuf->front = PREFIX##_internal_dec(rbuf, rbuf->front);                                                                                     \
	}                                                                                                                    \
	else {                                                                                                               \
		for (size_t i = rbuf->size; i > index; i--) {                                                                \
			rbuf->data[PREFIX##_internal_index(rbuf, i)] = rbuf->data[PREFIX##_internal_index(rbuf, i - 1)];     \
		}                                                                                                            \
		rbuf->back = PREFIX##_internal_inc(rbuf, rbuf->back);                                                              \
	}                                                                                                                    \
	rbuf->data[PREFIX##_internal_index(rbuf, index)] = value;                                                                \
	rbuf->size++;                                                                                                        \
	return 0;                                                                                                            \
}                                                                                                                            \
                                                                                                                             \
int PREFIX##_remove(PREFIX##_ring_buffer* rbuf, size_t index, TYPE* out) {                                                   \
	if (!rbuf) return 1;                                                                                                 \
	if (index >= rbuf->size) return 1;                                                                                   \
                                                                                                                             \
	if (out) *out = *PREFIX##_at(rbuf, index);                                                                            \
                                                                                                                             \
	size_t left = index;                                                                                                 \
	size_t right = rbuf->size - index;                                                                                   \
	if (left < right) {                                                                                                  \
		for (size_t i = index; i > 0; i--) {                                                                         \
			rbuf->data[PREFIX##_internal_index(rbuf, i)] = rbuf->data[PREFIX##_internal_index(rbuf, i - 1)];           \
		}                                                                                                            \
		rbuf->front = PREFIX##_internal_inc(rbuf, rbuf->front);                                                            \
	}                                                                                                                    \
	else {                                                                                                               \
		for (size_t i = index; i < rbuf->size - 1; i++) {                                                            \
			rbuf->data[PREFIX##_internal_index(rbuf, i)] = rbuf->data[PREFIX##_internal_index(rbuf, i + 1)];     \
		}                                                                                                            \
		rbuf->back = PREFIX##_internal_dec(rbuf, rbuf->back);                                                         \
	}                                                                                                                    \
                                                                                                                             \
	rbuf->size--;                                                                                                        \
                                                                                                                             \
	if (PREFIX##_is_empty(rbuf)) {                                                                                       \
		rbuf->front = 0;                                                                                             \
		rbuf->back = 0;                                                                                              \
	}                                                                                                                    \
                                                                                                                             \
	return 0;                                                                                                            \
}                                                                                                                            \
                                                                                                                             \
int PREFIX##_resize(PREFIX##_ring_buffer* rbuf, size_t size) {                                                               \
	if (!rbuf) return 1;                                                                                                 \
                                                                                                                             \
	if (size <= rbuf->size) {                                                                                            \
		rbuf->size = size;                                                                                           \
		if (rbuf->size == 0) { \
			rbuf->front = 0; \
			rbuf->back = 0; \
		} \
		else rbuf->back = PREFIX##_internal_index(rbuf, size - 1); \
		return 0;                                                                                                    \
	}                                                                                                                    \
	else if(PREFIX##_reserve(rbuf, size)) return 1;                                                                      \
                                                                                                                             \
	for (size_t i = rbuf->size; i < size; i++) rbuf->data[PREFIX##_internal_index(rbuf, i)] = (TYPE){0};                 \
	rbuf->size = size;                                                                                                   \
	rbuf->back = PREFIX##_internal_index(rbuf, size - 1); \
	return 0;                                                                                                            \
}                                                                                                                            \
                                                                                                                             \
PREFIX##_iter PREFIX##_iter_begin(const PREFIX##_ring_buffer* rbuf) {                                                        \
	return (PREFIX##_iter){rbuf, 0};                                                                                     \
}                                                                                                                            \
                                                                                                                             \
PREFIX##_iter PREFIX##_iter_end(const PREFIX##_ring_buffer* rbuf) {                                                          \
	return rbuf == NULL ? (PREFIX##_iter){NULL, 0} : (PREFIX##_iter){rbuf, rbuf->size};                                  \
}                                                                                                                            \
                                                                                                                             \
int PREFIX##_iter_next(PREFIX##_iter* iter, const TYPE** out) {                                                              \
	if (!iter || !out || !iter->buf) return 0;                                                                           \
	if (iter->index >= iter->buf->size) {                                                                                 \
		*out = NULL;                                                                                                 \
		return 0;                                                                                                    \
	}                                                                                                                    \
	*out = PREFIX##_const_at(iter->buf, iter->index);                                                                    \
	iter->index++;                                                                                                         \
	return 1;                                                                                                            \
}                                                                                                                            \
                                                                                                                             \
int PREFIX##_iter_prev(PREFIX##_iter* iter, const TYPE** out) {                                                              \
	if (!iter || !out || !iter->buf) return 0;                                                                           \
	if (iter->index == 0) {                                                                                              \
		*out = NULL;                                                                                                 \
		return 0;                                                                                                    \
	}                                                                                                                    \
	iter->index--;                                                                                                         \
	*out = PREFIX##_const_at(iter->buf, iter->index);                                                                    \
	return 1;                                                                                                            \
}                                                                                                                            \


#endif // RING_BUFFER_H
