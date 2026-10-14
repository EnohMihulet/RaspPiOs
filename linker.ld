ENTRY(entry_point)

MEMORY
{
	RAM (rwx) : ORIGIN = 0x80000, LENGTH = 0x3F600000
}

SECTIONS
{
	. = ORIGIN(RAM);

	__kernel_start = .;

	.text.entry_point : {
		KEEP(*(.text.entry_point))
	} > RAM

	.exception_vectors ALIGN(0x800) :
	{
		__vectors_start = .;
		KEEP(*(.exception_vectors))
		__vectors_end = .;
	} > RAM

	.text : ALIGN(16)
	{
		*(.text*)
	} > RAM

	.rodata : ALIGN(16)
	{
		*(.rodata*)
	} > RAM
	
	.data : ALIGN(16)
	{
		*(.data*)
	} > RAM

	.bss (NOLOAD) : ALIGN(16)
	{
		__bss_start = .;
		*(.bss*)
		*(COMMON)
		. = ALIGN(16);
		__bss_end = .;
	} > RAM

	. = ALIGN(16);
	
	__stack_bottom = .;
	
	. += 0x10000;

	__stack_top = .;
	
	__kernel_end = .;
	
	/DISCARD/ :
	{
	    *(.comment)
	    *(.note*)
	    *(.eh_frame*)
	}
}
