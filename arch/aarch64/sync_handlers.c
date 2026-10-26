#include "sync_handlers.h"
#include "handle_exceptions.h"
#include "thread.h"
#include "printk.h"

#define X(NAME, VALUE, HANDLER, MESSAGE) [VALUE] = {#NAME, MESSAGE, HANDLER},
const exception_info_t exception_info[EC_COUNT] = {
	EXCEPTION_CLASSES
};
#undef X

exception_frame_t* svc_handler(exception_frame_t * frame) {
	if (frame == NULL) panic("svc_handler frame NULL");

	exception_frame_t* return_frame = frame;
	svc_codes_t op = (uint32_t)(frame->esr_el1 & 0xffffu);
	
	switch (op) {
	case svc_thread_yield:
		return_frame = next_thread(frame);
		break;
	case svc_thread_exit:
		current_thread->state = THREAD_DEAD;
		return_frame = next_thread(frame);
		break;
	case svc_thread_block:
		current_thread->state = THREAD_BLOCKED;
		return_frame = next_thread(frame);
		break;
	default:
		kprintf("Invalid svc operand: 0x%x\r\n", (uint32_t)op);
		dump_exception_frame(frame);
		panic("Invalid svc operand");
	}

	return return_frame;
}

exception_frame_t* illegal_exec_handler(exception_frame_t* frame) {
	if (frame == NULL) panic("illegal_exec_handler frame NULL");

	uint64_t esr = frame->esr_el1;

	uint32_t ec  = ESR_EC(esr);
	// SPSR[20] is the saved illegal-state flag, not ESR[25] (instruction length).
	uint32_t illegal = EXTRACT_BITS(frame->spsr_el1, 20, 1);
	uint32_t aarch32 = EXTRACT_BITS(frame->spsr_el1, 4, 1);
	uint32_t mode = EXTRACT_BITS(frame->spsr_el1, 0, 4);

	const char* mode_name = "Unexpected mode for EL1 handler";
	if (aarch32) {
		mode_name = "AArch32 (different mode encoding)";
	} else {
		switch (mode) {
		case 0x0: mode_name = "EL0t, SP_EL0"; break;
		case 0x4: mode_name = "EL1t, SP_EL0"; break;
		case 0x5: mode_name = "EL1h, SP_EL1"; break;
		}
	}

	kprintf("Illegal execution state: EC=0x%x\r\n", ec);
	kprintf("  SPSR.IL=%u (saved illegal execution state)\r\n", illegal);
	kprintf("  nRW=%u M[3:0]=0x%x (%s)\r\n", aarch32, mode, mode_name);
	dump_exception_frame(frame);
	panic("Unhandled illegal execution state");
}

static void dump_data_abort(const exception_frame_t* frame, const char* origin) {
	uint64_t esr = frame->esr_el1;

	uint32_t ec  = ESR_EC(esr);
	uint32_t il  = ESR_IL(esr);
	uint32_t iss = ESR_ISS(esr);

	uint32_t dfsc  = EXTRACT_BITS(iss, 0, 6);  // [5:0]: data fault status code.
	uint32_t wnr   = EXTRACT_BITS(iss, 6, 1);  // [6]: 0 = read, 1 = write; cache maintenance has special rules.
	uint32_t s1ptw = EXTRACT_BITS(iss, 7, 1);  // [7]: stage-2 fault during a stage-1 table walk.
	uint32_t cm    = EXTRACT_BITS(iss, 8, 1);  // [8]: fault caused by cache maintenance.
	uint32_t ea    = EXTRACT_BITS(iss, 9, 1);  // [9]: implementation-defined external-abort classification.
	uint32_t fnv   = EXTRACT_BITS(iss, 10, 1); // [10]: FAR invalid for DFSC 0x10; otherwise RES0.
	uint32_t isv   = EXTRACT_BITS(iss, 24, 1); // [24]: instruction syndrome [23:14] is valid.

	// Baseline Cortex-A76 DFSC encodings; keep unknown codes visible numerically.
	const char* fault = "Unrecognized data fault";
	int level = -1;
	if (dfsc <= 0x03) {
		fault = "Address-size fault";
		level = dfsc & 3u;
	} else if (dfsc <= 0x07) {
		fault = "Translation fault";
		level = dfsc & 3u;
	} else if (dfsc >= 0x09 && dfsc <= 0x0b) {
		fault = "Access-flag fault";
		level = dfsc & 3u;
	} else if (dfsc >= 0x0d && dfsc <= 0x0f) {
		fault = "Permission fault";
		level = dfsc & 3u;
	} else if (dfsc == 0x10) {
		fault = "Synchronous external abort (not a table walk)";
	} else if (dfsc >= 0x14 && dfsc <= 0x17) {
		fault = "Synchronous external abort during table walk";
		level = dfsc & 3u;
	} else if (dfsc == 0x18) {
		fault = "Synchronous parity/ECC error (not a table walk)";
	} else if (dfsc >= 0x1c && dfsc <= 0x1f) {
		fault = "Synchronous parity/ECC error during table walk";
		level = dfsc & 3u;
	} else if (dfsc == 0x21) {
		fault = "Alignment fault";
	} else if (dfsc == 0x30) {
		fault = "TLB conflict abort";
	}

	kprintf("Data abort from %s EL: EC=0x%x IL=%u (%u-bit) ISS=0x%x\r\n", origin, ec, il, il ? 32u : 16u, iss);
	kprintf("  DFSC=0x%x (%s)", dfsc, fault);
	if (level >= 0) kprintf(" level=%d", level);
	kprintf("\r\n");
	kprintf("  WnR=%u (%s) CM=%u (cache maintenance)\r\n", wnr, cm ? "cache-maintenance-specific" : (wnr ? "write" : "read"), cm);
	kprintf("  S1PTW=%u (stage-2 fault during stage-1 table walk)\r\n", s1ptw);
	kprintf("  EA=%u (implementation-defined; external aborts only)\r\n", ea);
	kprintf("  FnV=%u (%s)\r\n", fnv, dfsc == 0x10 ? (fnv ? "FAR invalid" : "FAR valid") : "not applicable; reserved zero");
	kprintf("  ISV=%u (instruction syndrome %s)\r\n", isv, isv ? "valid" : "not valid");

	// These fields must not be interpreted when ISV is zero.
	if (isv) {
		uint32_t ar  = EXTRACT_BITS(iss, 14, 1); // [14]: acquire/release semantics.
		uint32_t sf  = EXTRACT_BITS(iss, 15, 1); // [15]: 0 = 32-bit, 1 = 64-bit register.
		uint32_t srt = EXTRACT_BITS(iss, 16, 5); // [20:16]: transfer register; 31 = zero register.
		uint32_t sse = EXTRACT_BITS(iss, 21, 1); // [21]: sign-extension for a load.
		uint32_t sas = EXTRACT_BITS(iss, 22, 2); // [23:22]: log2(access size in bytes).

		kprintf("  SAS=%u (%u bytes) SSE=%u (sign-extend load)\r\n", sas, 1u << sas, sse);
		kprintf("  SRT=%u (%s) SF=%u (%u-bit register)\r\n",
			srt, srt == 31 ? "zero register" : "general-purpose register", sf, sf ? 64u : 32u);
		kprintf("  AR=%u (acquire/release semantics)\r\n", ar);
	}

	dump_exception_frame(frame);
}

exception_frame_t* dabt_lower_handler(exception_frame_t* frame) {
	if (frame == NULL) panic("dabt_lower_handler frame NULL");

	dump_data_abort(frame, "lower");
	panic("Unhandled data abort from lower EL");
}

exception_frame_t* dabt_current_handler(exception_frame_t* frame) {
	if (frame == NULL) panic("dabt_current_handler frame NULL");

	dump_data_abort(frame, "current");
	panic("Unhandled data abort from current EL");
}

static void dump_instruction_abort(const exception_frame_t* frame, const char* origin) {
	uint64_t esr = frame->esr_el1;

	uint32_t ec  = ESR_EC(esr);
	uint32_t il  = ESR_IL(esr); // Instruction aborts report IL=1; not the fetched instruction's size.
	uint32_t iss = ESR_ISS(esr);

	uint32_t ifsc  = EXTRACT_BITS(iss, 0, 6);  // [5:0]: instruction fault status code.
	uint32_t s1ptw = EXTRACT_BITS(iss, 7, 1);  // [7]: stage-2 fault during a stage-1 translation-table walk. Not applicable to your EL1 handler.
	uint32_t ea    = EXTRACT_BITS(iss, 9, 1);  // [9]: implementation-defined external-abort classification.
	uint32_t fnv   = EXTRACT_BITS(iss, 10, 1); // [10]: for IFSC 0x10: 1 = FAR invalid, 0 = FAR valid. Otherwise reserved zero.

	// Baseline Cortex-A76 IFSC encodings.
	const char* fault = "Unrecognized instruction fault";
	int level = -1;
	if (ifsc <= 0x03) {
		fault = "Address-size fault";
		level = ifsc & 3u;
	} else if (ifsc <= 0x07) {
		fault = "Translation fault";
		level = ifsc & 3u;
	} else if (ifsc >= 0x09 && ifsc <= 0x0b) {
		fault = "Access-flag fault";
		level = ifsc & 3u;
	} else if (ifsc >= 0x0d && ifsc <= 0x0f) {
		fault = "Permission fault";
		level = ifsc & 3u;
	} else if (ifsc == 0x10) {
		fault = "Synchronous external abort (not a table walk)";
	} else if (ifsc >= 0x14 && ifsc <= 0x17) {
		fault = "Synchronous external abort during table walk";
		level = ifsc & 3u;
	} else if (ifsc == 0x18) {
		fault = "Synchronous parity/ECC error (not a table walk)";
	} else if (ifsc >= 0x1c && ifsc <= 0x1f) {
		fault = "Synchronous parity/ECC error during table walk";
		level = ifsc & 3u;
	} else if (ifsc == 0x30) {
		fault = "TLB conflict abort";
	}

	kprintf("Instruction abort from %s EL: EC=0x%x IL=%u (expected 1) ISS=0x%x\r\n", origin, ec, il, iss);
	kprintf("  IFSC=0x%x (%s)", ifsc, fault);
	if (level >= 0) kprintf(" level=%d", level);
	kprintf("\r\n");
	kprintf("  S1PTW=%u (stage-2 fault during stage-1 table walk; not applicable at EL1)\r\n", s1ptw);
	kprintf("  EA=%u (implementation-defined; external aborts only)\r\n", ea);
	kprintf("  FnV=%u (%s)\r\n", fnv, ifsc == 0x10 ? (fnv ? "FAR invalid" : "FAR valid") : "not applicable; reserved zero");
	dump_exception_frame(frame);
}

exception_frame_t* iabt_lower_handler(exception_frame_t* frame) {
	if (frame == NULL) panic("iabt_lower_handler frame NULL");

	dump_instruction_abort(frame, "lower");
	panic("Unhandled instruction abort from lower EL");
}

exception_frame_t* iabt_current_handler(exception_frame_t* frame) {
	if (frame == NULL) panic("iabt_current_handler frame NULL");

	dump_instruction_abort(frame, "current");
	panic("Unhandled instruction abort from current EL");
}

exception_frame_t* brk64_handler(exception_frame_t* frame) {
	if (frame == NULL) panic("brk64_handler frame NULL");

	uint64_t esr = frame->esr_el1;
	uint32_t ec = ESR_EC(esr);
	uint32_t iss = ESR_ISS(esr);

	// ISS[15:0]: the BRK instruction's unsigned 16-bit immediate (debug comment).
	uint32_t immediate = EXTRACT_BITS(iss, 0, 16);

	kprintf("Software breakpoint (BRK): EC=0x%x ISS=0x%x\r\n", ec, iss);
	kprintf("  Immediate=0x%x (%u) ELR=0x%lx\r\n",
		immediate, immediate, frame->elr_el1);
	dump_exception_frame(frame);
	panic("Unhandled software breakpoint");
}

exception_frame_t* pc_align_handler(exception_frame_t* frame) {
	if (frame == NULL) panic("pc_align_handler frame NULL");

	uint32_t ec = ESR_EC(frame->esr_el1);

	// ISS is reserved zero.
	// FAR_EL1 contains the misaligned instruction address.
	kprintf("Program counter alignment fault: EC=0x%x\r\n", ec);
	kprintf("  ELR=0x%lx FAR=0x%lx (misaligned instruction address)\r\n", frame->elr_el1, frame->far_el1);
	dump_exception_frame(frame);
	panic("Unhandled program counter alignment fault");
}

exception_frame_t* sp_align_handler(exception_frame_t* frame) {
	if (frame == NULL) panic("sp_align_handler frame NULL");

	uint32_t ec = ESR_EC(frame->esr_el1);

	// With SP alignment checking enabled, an SP-based memory access requires 16-byte alignment.
	// ISS is reserved zero and FAR is not supplied for this fault.
	// SPSR[4] selects AArch32 (1) or AArch64 (0); [3:0] is the AArch64 mode.
	uint32_t aarch32 = EXTRACT_BITS(frame->spsr_el1, 4, 1);
	uint32_t mode = EXTRACT_BITS(frame->spsr_el1, 0, 4);
	const char* mode_name = "Unexpected mode for EL1 handler";
	if (aarch32) {
		mode_name = "AArch32 (different mode encoding)";
	} else {
		switch (mode) {
		case 0x0: mode_name = "EL0t, SP_EL0"; break;
		case 0x4: mode_name = "EL1t, SP_EL0"; break;
		case 0x5: mode_name = "EL1h, SP_EL1"; break;
		}
	}

	kprintf("Stack pointer alignment fault: EC=0x%x\r\n", ec);
	kprintf("  Saved SP=0x%lx (required alignment: 16 bytes)\r\n", frame->sp);
	kprintf("  SPSR=0x%lx nRW=%u M[3:0]=0x%x (%s)\r\n", frame->spsr_el1, aarch32, mode, mode_name);
	kprintf("  FAR is not valid fault-address data for this exception\r\n");
	dump_exception_frame(frame);
	panic("Unhandled stack pointer alignment fault");
}
