#pragma once

#include "handle_exceptions.h"

typedef exception_frame_t* (*exception_handler_t)(exception_frame_t*);

exception_frame_t* svc_handler(exception_frame_t* frame);

exception_frame_t* illegal_exec_handler(exception_frame_t* frame);
exception_frame_t* dabt_lower_handler(exception_frame_t* frame);
exception_frame_t* dabt_current_handler(exception_frame_t* frame);
exception_frame_t* iabt_lower_handler(exception_frame_t* frame);
exception_frame_t* iabt_current_handler(exception_frame_t* frame);
exception_frame_t* brk64_handler(exception_frame_t* frame);
exception_frame_t* pc_align_handler(exception_frame_t* frame);
exception_frame_t* sp_align_handler(exception_frame_t* frame);

#define EXCEPTION_CLASSES                                                                         \
	X(EC_UNKNOWN,            0x00, NULL, "Unknown or undefined instruction")                  \
	X(EC_WFX,                0x01, NULL, "Trapped WFI/WFE instruction")                       \
	X(EC_FP_ASIMD,           0x07, NULL, "Trapped floating-point or SIMD access")             \
	X(EC_ILLEGAL,            0x0E, illegal_exec_handler, "Illegal execution state")           \
                                                                                                  \
	X(EC_SVC64,              0x15, svc_handler, "Supervisor call (AArch64)")                  \
	X(EC_HVC64,              0x16, NULL, "Hypervisor call (AArch64)")                         \
	X(EC_SMC64,              0x17, NULL, "Secure monitor call (AArch64)")                     \
	X(EC_SYS64,              0x18, NULL, "Trapped system instruction")                        \
                                                                                                  \
	X(EC_IABT_LOWER,         0x20, iabt_lower_handler, "Instruction abort from lower EL")     \
	X(EC_IABT_CURRENT,       0x21, iabt_current_handler, "Instruction abort from current EL") \
	X(EC_PC_ALIGN,           0x22, pc_align_handler, "Program counter alignment fault")       \
	X(EC_DABT_LOWER,         0x24, dabt_lower_handler, "Data abort from lower EL")            \
	X(EC_DABT_CURRENT,       0x25, dabt_current_handler, "Data abort from current EL")        \
	X(EC_SP_ALIGN,           0x26, sp_align_handler, "Stack pointer alignment fault")         \
	X(EC_FP_EXC64,           0x2C, NULL, "Floating-point exception (AArch64)")                \
                                                                                                  \
	X(EC_BREAKPOINT_LOWER,   0x30, NULL, "Hardware breakpoint from lower EL")                 \
	X(EC_BREAKPOINT_CURRENT, 0x31, NULL, "Hardware breakpoint from current EL")               \
	X(EC_SOFTSTEP_LOWER,     0x32, NULL, "Software step from lower EL")                       \
	X(EC_SOFTSTEP_CURRENT,   0x33, NULL, "Software step from current EL")                     \
	X(EC_WATCHPOINT_LOWER,   0x34, NULL, "Hardware watchpoint from lower EL")                 \
	X(EC_WATCHPOINT_CURRENT, 0x35, NULL, "Hardware watchpoint from current EL")               \
	X(EC_BRK64,              0x3C, brk64_handler, "Software breakpoint (BRK instruction)")

#define X(NAME, VALUE, HANDLER, MESSAGE) NAME = VALUE,
typedef enum {
	EXCEPTION_CLASSES
	EC_COUNT = 0x40,
} exception_class_t;
#undef X

typedef struct {
	const char* name;
	const char* message;
	exception_handler_t handler;
} exception_info_t;

extern const exception_info_t exception_info[EC_COUNT];

#define ESR_EC(esr)  (((esr) >> 26) & 0x3fu)
#define ESR_IL(esr)  (((esr) >> 25) & 0x1u)
#define ESR_ISS(esr) ((esr) & 0x01ffffffu)

#define EXTRACT_BITS(value, lsb, width) (((uint64_t)(value) >> (lsb)) & (UINT64_MAX >> (64u - (width))))

typedef enum {
	svc_thread_yield    = 0,
	svc_thread_exit     = 1,
	svc_thread_block    = 2,
	svc_thread_sleeping = 3,
} svc_codes_t;
