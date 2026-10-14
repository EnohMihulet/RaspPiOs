.cpu cortex-a76
.text

.global entry_point
.global exception_vectors
.extern __vectors_start
.extern __vectors_end
.extern __bss_start
.extern __bss_end
.extern __kernel_end
.extern __stack_bottom
.extern __stack_top

.extern kernel_main

.section .text.entry_point, "ax", %progbits
.weak entry_point
.type entry_point, %function
.p2align 2

entry_point:
	msr	daifset, #0xf // Disable interrupts

	// Check current EL
	mrs	x4, CurrentEL
	lsr	x4, x4, #2

	// Compare current EL to EL1
	cmp	x4, #1
	beq	el1_entry

el2_to_el1:
	// Confirm EL2
	cmp	x4, #2
	bne	hang

	// Configure EL2 so EL1 runs in AArch64
	mov	x4, #(1 << 31)
	msr	HCR_EL2, x4

	// Prepare EL1: MMU and caches off, little-endian
	ldr	x4, =0x30D00800
	msr	SCTLR_EL1, x4

	// Enable EL1 access to the physical counter and timer registers
	mrs x4, CNTHCTL_EL2
	orr x4, x4, #3 // enables bits 0 and 1
	msr CNTHCTL_EL2, x4

	// Where ERET should land
	adr	x4, el1_entry
	msr	ELR_EL2, x4
	
	// Prepare EL1 execution state
	mov	x4, #0x3C5 // Enter EL1h with exceptions masked
	msr	SPSR_EL2, x4

	eret

el1_entry:
	msr	SPSel, #1 // Select SP_EL1

	// Park secondary cores
	mrs	x4, mpidr_el1
	and	x4, x4, #0xff
	cbnz	x4, park

	// Create stack
	ldr	x4, =__stack_top
	mov	sp, x4

// Clear .bss
zero_bss:
	ldr	x4, =__bss_start
	ldr	x5, =__bss_end

zero_bss_loop:
	cmp	x4, x5
	bcc	zero_bss_store
	b	install_exception_vectors

zero_bss_store:
	str	xzr, [x4], #8
	b	zero_bss_loop

install_exception_vectors:
	ldr	x4, =exception_vectors
	msr	VBAR_EL1, x4
	isb

// Jump to kerel_main
call_main:
	bl	kernel_main

hang:
	wfe
	b hang

park:
	wfe
	b park

.size entry_point, . - entry_point
