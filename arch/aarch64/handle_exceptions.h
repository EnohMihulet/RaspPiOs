#ifndef HANDLE_EXCEPTIONS_H
#define HANDLE_EXCEPTIONS_H

#include <stddef.h>
#include <stdint.h>

extern volatile uint64_t timer_ticks;

typedef struct {
	uint64_t r[31];
	// Interrupted SP. Restored for SP_EL0; diagnostic-only for SP_EL1.
	uint64_t sp;

	uint64_t elr_el1;
	uint64_t spsr_el1;
	
	// Raw snapshots: ESR/FAR are not interrupt IDs or always-valid fault data.
	uint64_t esr_el1;
	uint64_t far_el1;
} exception_frame;

// Assembly uses these byte offsets and requires a 16-byte-aligned frame size.
_Static_assert(offsetof(exception_frame, r) == 0, "exception frame GPR offset");
_Static_assert(offsetof(exception_frame, sp) == 248, "exception frame SP offset");
_Static_assert(offsetof(exception_frame, elr_el1) == 256, "exception frame ELR offset");
_Static_assert(offsetof(exception_frame, spsr_el1) == 264, "exception frame SPSR offset");
_Static_assert(offsetof(exception_frame, esr_el1) == 272, "exception frame ESR offset");
_Static_assert(offsetof(exception_frame, far_el1) == 280, "exception frame FAR offset");
_Static_assert(sizeof(exception_frame) == 288, "exception frame size");

void dump_exception_frame(exception_frame* frame);

void sync_handler(exception_frame* frame);

void irq_handler(exception_frame* frame);

_Noreturn void panic(const char* message);

#endif
