#include <convert.h>
#include <usart.h>

#include "printk.h"

static kprintf_output_fn output_char = uart_putc;

void kprintf_set_output(kprintf_output_fn output) {
	output_char = output ? output : uart_putc;
}

static void print_string(const char* str) {
	if (!str) str = "(null)";
	while (*str) output_char(*str++);
}

void vkprintf(const char* fmt, va_list args) {
	for (const char* p = fmt; *p; ++p) {
		if (*p != '%') { output_char(*p); continue; }

		const char* start = p;
		if (*(p+1) == '%') { output_char('%'); ++p; continue; }

		enum { LEN_NONE, LEN_L, LEN_LL } len = LEN_NONE;
		++p;
		if (*p == 'l') {
			len = LEN_L;
			++p;
			if (*p == 'l') { len = LEN_LL; ++p; }
		}

		// A trailing % or %ll must not advance past the string terminator.
		if (!*p) { print_string(start); break; }
		if (len != LEN_NONE && *p != 'd' && *p != 'i' &&
				*p != 'u' && *p != 'x' && *p != 'X') {
			while (start <= p) output_char(*start++);
			continue;
		}

		char buffer[CONVERT_BUFFER_SIZE];
		switch (*p) {
		case 'c': {
			int c = va_arg(args, int);
			output_char((char)c);
			break;
		}
		case 's': {
			print_string(va_arg(args, const char*));
			break;
		}
		case 'd': case 'i': {
			intmax_t value;
			if (len == LEN_LL) value = va_arg(args, long long);
			else if (len == LEN_L) value = va_arg(args, long);
			else value = va_arg(args, int);
			itos(value, buffer, sizeof(buffer));
			print_string(buffer);
			break;
		}
		case 'u': case 'x': case 'X': {
			uintmax_t value;
			if (len == LEN_LL) value = va_arg(args, unsigned long long);
			else if (len == LEN_L) value = va_arg(args, unsigned long);
			else value = va_arg(args, unsigned int);
			if (*p == 'u') utos(value, buffer, sizeof(buffer));
			else utos_hex(value, buffer, sizeof(buffer));
			if (*p == 'X') {
				for (char* digit = buffer; *digit; ++digit) {
					if (*digit >= 'a' && *digit <= 'f') *digit -= 'a' - 'A';
				}
			}
			print_string(buffer);
			break;
		}
		case 'p': {
			ptos_hex(va_arg(args, void*), buffer, sizeof(buffer));
			print_string(buffer);
			break;
		}
		default:
			while (start <= p) output_char(*start++);
			break;
		}
	}
}

void kprintf(const char* fmt, ...) {
	va_list args;
	va_start(args, fmt);
	vkprintf(fmt, args);
	va_end(args);
}
