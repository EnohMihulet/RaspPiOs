.extern sync_handler
.extern irq_handler

default_handler:
infinite_loop:
	wfe
	b	infinite_loop

.section .exception_vectors, "ax", %progbits
.balign 2048
.type exception_vectors, %object
.global exception_vectors
exception_vectors:
	b sync_sp0
	.space 128-4
	b irq_sp0
	.space 128-4
	b fiq_sp0
	.space 128-4
	b serr_sp0
	.space 128-4

	b sync_spx
	.space 128-4
	b irq_spx
	.space 128-4
	b fiq_spx
	.space 128-4
	b serr_spx
	.space 128-4

	b sync_a64
	.space 128-4
	b irq_a64
	.space 128-4
	b fiq_a64
	.space 128-4
	b serr_a64
	.space 128-4

	b sync_a32
	.space 128-4
	b irq_a32
	.space 128-4
	b fiq_a32
	.space 128-4
	b serr_a32
	.space 128-4

.size exception_vectors, . - exception_vectors

.macro save_exception_frame
	sub	sp, sp, #288

	stp	x0, x1, [sp, #0*16]   
	stp	x2, x3, [sp, #1*16]
	stp	x4, x5, [sp, #2*16]
	stp	x6, x7, [sp, #3*16]
	stp	x8, x9, [sp, #4*16]
	stp	x10, x11, [sp, #5*16]
	stp	x12, x13, [sp, #6*16]
	stp	x14, x15, [sp, #7*16]
	stp	x16, x17, [sp, #8*16]
	stp	x18, x19, [sp, #9*16]
	stp	x20, x21, [sp, #10*16]
	stp	x22, x23, [sp, #11*16]
	stp	x24, x25, [sp, #12*16]
	stp	x26, x27, [sp, #13*16]
	stp	x28, x29, [sp, #14*16]
	str	x30, [sp, #15*16]

	mrs	x1, ELR_EL1
	mrs	x2, SPSR_EL1
	stp	x1, x2, [sp, #256]

	add	x0, sp, #288

	// SPSR_EL1 bit 0:
	// 0 = EL0t (or EL1t), uses SP_EL0
	// 1 = EL1h, uses SP_EL1
	tbnz	x2, #0, .Lsave_sp\@
	mrs	x0, SP_EL0

.Lsave_sp\@:
	str	x0, [sp, #248]

	mrs	x1, ESR_EL1
	mrs	x2, FAR_EL1
	stp	x1, x2, [sp, #272]

	mov	x0, sp // First C argument: exception_frame *.
.endm

.macro restore_exception_frame
	// x0 = pointer returned by irq_handler.
	// Switch to the selected exception frame.
	mov	sp, x0
	ldp	x1, x2, [sp, #256]
	msr	ELR_EL1, x1
	msr	SPSR_EL1, x2

	// SPSR_EL1 bit 0:
	// 0 = EL0t (or EL1t), uses SP_EL0
	// 1 = EL1h, uses SP_EL1
	tbnz	x2, #0, .Lrestore_sp\@

	ldr	x1, [sp, #248]
	msr	SP_EL0, x1
.Lrestore_sp\@:
	// Restore general-purpose registers.
	ldp	x0, x1, [sp, #0*16]
	ldp	x2, x3, [sp, #1*16]
	ldp	x4, x5, [sp, #2*16]
	ldp	x6, x7, [sp, #3*16]
	ldp	x8, x9, [sp, #4*16]
	ldp	x10, x11, [sp, #5*16]
	ldp	x12, x13, [sp, #6*16]
	ldp	x14, x15, [sp, #7*16]
	ldp	x16, x17, [sp, #8*16]
	ldp	x18, x19, [sp, #9*16]
	ldp	x20, x21, [sp, #10*16]
	ldp	x22, x23, [sp, #11*16]
	ldp	x24, x25, [sp, #12*16]
	ldp	x26, x27, [sp, #13*16]
	ldp	x28, x29, [sp, #14*16]
	ldr	x30, [sp, #15*16]

	// Restore selected kernel stack position.
	add	sp, sp, #288
.endm

sync_sp0:
sync_spx:
sync_a64:
	save_exception_frame
	bl	sync_handler
	restore_exception_frame
	eret

irq_sp0:
irq_spx:
irq_a64:
	save_exception_frame
	bl	irq_handler
	restore_exception_frame
	eret

sync_a32:
irq_a32:
fiq_sp0:
fiq_spx:
fiq_a64:
fiq_a32:
	b default_handler

serr_sp0:
serr_spx:
serr_a64:
serr_a32:
	b default_handler
