#include "usart.h"

void uart_init(void) {
	// Let the firmware's last characters finish before reconfiguring UART10.
	while (UART10->FR & UART_FR_BUSY) {}

	// Disable UART
	UART10->CR = 0;

	// Disable UART interrupts
	UART10->IMSC = 0;

	// Clear pending interrupts
	UART10->ICR = 0x7FFu;

	// Keep the firmware's baud divisors: its UART10 clock is not configured

	// 8 data bits, 1 stop bit, no parity
	// Enable transmit and receive FIFOs
	UART10->LCRH = UART_LCRH_WLEN_8 | UART_LCRH_FEN;

	// Enable UART, transmit, & receive
	UART10->CR = UART_CR_UARTEN | UART_CR_TXE | UART_CR_RXE;
}

void uart_putc(char c) {
	while (UART10->FR & UART_FR_TXFF) {}

	UART10->DR = (uint32_t)c;
}

void uart_puts(char* s) {
	while (*s) {
		if (*s == '\n')
			uart_putc('\r');

		uart_putc(*s++);
	}
}

void uart_puthex(uint64_t value) {
	const char* hex = "0123456789ABCDEF";

	uart_puts("0x");

	for (int i = 60; i >= 0; i -= 4) {
		uart_putc(hex[(value >> i) & 0xF]);
	}
}
