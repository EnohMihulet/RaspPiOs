#pragma once

#include <stdint.h>

#define UART10_BASE 0x000000107D001000ULL

#define UART_CLOCK 9216000 // 9,216 MHz


typedef struct {
	uint32_t DR;            // 0x000
	uint32_t RSR_ECR;       // 0x004

	uint32_t RESERVED0[4];  // 0x008 - 0x014

	uint32_t FR;            // 0x018

	uint32_t RESERVED1;     // 0x01C

	uint32_t ILPR;          // 0x020
	uint32_t IBRD;          // 0x024
	uint32_t FBRD;          // 0x028
	uint32_t LCRH;          // 0x02C
	uint32_t CR;            // 0x030
	uint32_t IFLS;          // 0x034
	uint32_t IMSC;          // 0x038
	uint32_t RIS;           // 0x03C
	uint32_t MIS;           // 0x040
	uint32_t ICR;           // 0x044
	uint32_t DMACR;         // 0x048

	uint32_t RESERVED2[13]; // 0x04C - 0x07C

	uint32_t TCR;           // 0x080
	uint32_t ITIP;          // 0x084
	uint32_t ITOP;          // 0x088
	uint32_t TDR;           // 0x08C
} UART_typedef;


#define UART10 ((volatile UART_typedef*)UART10_BASE)


typedef enum {
	UART_FR_BUSY = (1u << 3),  // UART is currently transmitting data
	UART_FR_RXFE = (1u << 4),  // Receive FIFO empty
	UART_FR_TXFF = (1u << 5),  // Transmit FIFO full
	UART_FR_RXFF = (1u << 6),  // Receive FIFO full
	UART_FR_TXFE = (1u << 7),  // Transmit FIFO empty
} UART_FR_bits_t;

typedef enum {
	UART_LCRH_BRK    = (1u << 0), // Send break
	UART_LCRH_PEN    = (1u << 1), // Enable parity
	UART_LCRH_EPS    = (1u << 2), // Select even parity
	UART_LCRH_STP2   = (1u << 3), // Use two stop bits
	UART_LCRH_FEN    = (1u << 4), // Enable FIFOs
	UART_LCRH_WLEN_5 = (0u << 5), // 5-bit words
	UART_LCRH_WLEN_6 = (1u << 5), // 6-bit words
	UART_LCRH_WLEN_7 = (2u << 5), // 7-bit words
	UART_LCRH_WLEN_8 = (3u << 5), // 8-bit words
	UART_LCRH_SPS    = (1u << 7), // Enable stick parity
} UART_LCRH_bits_t;

typedef enum {
	UART_CR_UARTEN = (1u << 0),  // Enable UART
	UART_CR_LBE    = (1u << 7),  // Enable loopback
	UART_CR_TXE    = (1u << 8),  // Enable transmitter
	UART_CR_RXE    = (1u << 9),  // Enable receiver
	UART_CR_RTS    = (1u << 11), // Request to send
	UART_CR_RTSEN  = (1u << 14), // Enable RTS hardware flow control
	UART_CR_CTSEN  = (1u << 15), // Enable CTS hardware flow control
} UART_CR_bits_t;

// Not yet implemented:
// DR	 - Data/error bit definitions
// RSR/ECR - Receive error status/clear
// ILPR   - IrDA low-power divisor
// IFLS   - FIFO interrupt threshold configuration
// IMSC   - Interrupt masking
// RIS	- Raw interrupt status
// MIS	- Masked interrupt status
// ICR	- Interrupt clearing
// DMACR  - DMA configuration
// TCR	- Test control
// ITIP   - Integration test input
// ITOP   - Integration test output
// TDR	- Test data


void uart_init(void);

void uart_putc(char c);
void uart_puts(char* s);
void uart_puthex(uint64_t value);
