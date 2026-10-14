#include "usart.h"

void uart_init(void) {
	// Disable UART
	UART10->CR = 0;

	// Disable UART interrupts
	UART10->IMSC = 0;

	// Clear pending interrupts
	UART10->ICR = 0x7FFu;

	/*
	* UART10 clock: 9,216 MHz
	* Baud divisor: 9216000 / (16 * 115200) = 5
	*/
	UART10->IBRD = UART_CLOCK / (16 * 115200); // 5

	UART10->FBRD = 0;

	// 8-bit words & enable FIFOs
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
