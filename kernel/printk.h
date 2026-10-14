#ifndef RASPPIOS_PRINTK_H
#define RASPPIOS_PRINTK_H

#include <stdarg.h>

typedef void (*kprintf_output_fn)(char c);

// Defaults to uart_putc; passing NULL restores UART output.
// Initialize the chosen device before printing. Not synchronized across cores.
void kprintf_set_output(kprintf_output_fn output);

// Supports %c, %s, %d, %i, %u, %x, %X, %p, %% and l/ll integer modifiers.
// No width, precision, or flags. Unknown/incomplete conversions print literally.
// %p includes 0x; %x/%X do not. NULL strings print as "(null)".
void kprintf(const char* fmt, ...);
void vkprintf(const char* fmt, va_list args);

#endif
