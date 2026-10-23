#pragma once

#include <stddef.h>
#include <stdint.h>

extern volatile uint64_t timer_ticks;

typedef struct {
	uint64_t x[31];
	// Interrupted SP. Restored for SP_EL0; diagnostic-only for SP_EL1.
	uint64_t sp;

	uint64_t elr_el1;
	uint64_t spsr_el1;
	
	// Raw snapshots: ESR/FAR are not interrupt IDs or always-valid fault data.
	uint64_t esr_el1;
	uint64_t far_el1;
} exception_frame_t;

// Assembly uses these byte offsets and requires a 16-byte-aligned frame size.
_Static_assert(offsetof(exception_frame_t, x) == 0, "exception frame GPR offset");
_Static_assert(offsetof(exception_frame_t, sp) == 248, "exception frame SP offset");
_Static_assert(offsetof(exception_frame_t, elr_el1) == 256, "exception frame ELR offset");
_Static_assert(offsetof(exception_frame_t, spsr_el1) == 264, "exception frame SPSR offset");
_Static_assert(offsetof(exception_frame_t, esr_el1) == 272, "exception frame ESR offset");
_Static_assert(offsetof(exception_frame_t, far_el1) == 280, "exception frame FAR offset");
_Static_assert(sizeof(exception_frame_t) == 288, "exception frame size");

void dump_exception_frame_t(exception_frame_t* frame);

exception_frame_t* sync_handler(exception_frame_t* frame);

exception_frame_t* irq_handler(exception_frame_t* frame);

_Noreturn void panic(const char* message);

void enable_irqs(void);
void disable_irqs(void);
