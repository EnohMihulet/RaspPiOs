#pragma once

#include <stdint.h>

//
// BASE ADDRESSES
//

#define GIC_BASE 0x107FFF8000ULL

#define GICD_BASE (GIC_BASE + 0x1000ULL)
#define GICC_BASE (GIC_BASE + 0x2000ULL)

//
// GICD REGISTER STRUCT
//

typedef struct {
	uint32_t CTLR;            // 0x000
	uint32_t TYPER;           // 0x004
	uint32_t IIDR;            // 0x008

	uint32_t RESERVED0[29];   // 0x00C - 0x07F

	uint32_t IGROUPR[32];     // 0x080 - 0x0FF
	uint32_t ISENABLER[32];   // 0x100 - 0x17F
	uint32_t ICENABLER[32];   // 0x180 - 0x1FF
	uint32_t ISPENDR[32];     // 0x200 - 0x27F
	uint32_t ICPENDR[32];     // 0x280 - 0x2FF
	uint32_t ISACTIVER[32];   // 0x300 - 0x37F
	uint32_t ICACTIVER[32];   // 0x380 - 0x3FF
	uint32_t IPRIORITYR[255]; // 0x400 - 0x7FB

	uint32_t RESERVED1;       // 0x7FC

	uint32_t ITARGETSR[255];  // 0x800 - 0xBFB

	uint32_t RESERVED2;       // 0xBFC

	uint32_t ICFGR[64];       // 0xC00 - 0xCFF
} GICD_typedef;

#define GICD ((volatile GICD_typedef*)GICD_BASE)

//
// GICD ENUMS AND MASKS
//

// Distributor Control Register (Non-secure)
typedef enum {
	GICD_CTLR_ENABLEGRP1 = (1u << 0), // Enable forwarding Group 1 interrupts
} GICD_CTLR_bits_t;

// Interrupt Controller Type Register
typedef enum {
	GICD_TYPER_ITLINESNUMBER = (0x1Fu << 0),  // Number of supported 32-intrrupt blocks, minus 1
	GICD_TYPER_CPUNUMBER     = (0x7u  << 5),  // Number of CPU interfaces, minus 1
	GICD_TYPER_SECURITYEXTN  = (1u    << 10), // Whether security extensions are implemented
	GICD_TYPER_LSPI          = (0x1Fu << 11), // Lockable shared peripheral interrupt capability
} GICD_TYPER_mask_t;

// Distributor Implementer Identification Register
typedef enum {
	GICD_IIDR_IMPLEMENTER = (0xFFFu << 0),  // Manufacturer idenfification
	GICD_IIDR_REVISION    = (0xFu   << 12), // Minor hardware revision
	GICD_IIDR_VARIANT     = (0xFu   << 16), // Major hardware revision
	GICD_IIDR_PRODUCTID   = (0xFFu  << 24), // Controller product identifier
} GICD_IIDR_mask_t;


// Interrupt ID -> 32-bit register index
#define GICD_IRQ_REG(irq) ((irq) / 32u)

// Interrupt ID -> bit mask inside that register
#define GICD_IRQ_BIT(irq) (1u << ((irq) % 32u))

// GICD_IPRIORITYR - Priority
#define GICD_PRIORITY_REG(irq)   ((irq) / 4u)
#define GICD_PRIORITY_SHIFT(irq) (((irq) % 4u) * 8u)
#define GICD_PRIORITY_MASK(irq)  (0xFFu << GICD_PRIORITY_SHIFT(irq))

// GICD_ITARGETSR - CPU targeting
typedef enum {
	GICD_TARGET_CPU0 = (1u << 0),
	GICD_TARGET_CPU1 = (1u << 1),
	GICD_TARGET_CPU2 = (1u << 2),
	GICD_TARGET_CPU3 = (1u << 3),
} GICD_TARGET_mask_t;

#define GICD_TARGET_REG(irq)   ((irq) / 4u)
#define GICD_TARGET_SHIFT(irq) (((irq) % 4u) * 8u)
#define GICD_TARGET_MASK(irq)  (0xFFu << GICD_TARGET_SHIFT(irq))

// GICD_ICFGR - Trigger configuration
typedef enum {
	GICD_CONFIG_LEVEL = 0b00u,
	GICD_CONFIG_EDGE  = 0b10u,
} GICD_CONFIG_t;

#define GICD_CONFIG_REG(irq)   ((irq) / 16u)
#define GICD_CONFIG_SHIFT(irq) (((irq) % 16u) * 2u)
#define GICD_CONFIG_MASK(irq)  (0x3u << GICD_CONFIG_SHIFT(irq))


//
// GICC REGISTER STRUCT
//

typedef struct {
	uint32_t CTLR;           // 0x0000
	uint32_t PMR;            // 0x0004
	uint32_t BPR;            // 0x0008
	uint32_t IAR;            // 0x000C
	uint32_t EOIR;           // 0x0010
	uint32_t RPR;            // 0x0014
	uint32_t HPPIR;          // 0x0018
	uint32_t ABPR;           // 0x001C
	uint32_t AIAR;           // 0x0020
	uint32_t AEOIR;          // 0x0024
	uint32_t AHPPIR;         // 0x0028
	
	uint32_t RESERVED0[41];  // 0x002C - 0x00CF
	
	uint32_t APR0;           // 0x00D0
	
	uint32_t RESERVED1[3];   // 0x00D4 - 0x00DF
	
	uint32_t NSAPR0;         // 0x00E0
	
	uint32_t RESERVED2[6];   // 0x00E4 - 0x00FB
	
	uint32_t IIDR;           // 0x00FC
	
	uint32_t RESERVED3[960]; // 0x0100 - 0x0FFF
	
	uint32_t DIR;            // 0x1000
} GICC_typedef;

#define GICC ((volatile GICC_typedef*)GICC_BASE)

//
// GICC ENUMTS AND MASKS
//

// GICC_CTLR - CPU Interface Control Register
typedef enum {
    GICC_CTLR_ENABLEGRP1 = (1u << 0), // ENables Group 1 interrupt delivery
    GICC_CTLR_EOIMODENS  = (1u << 9), // Controls how interrupts are completed
} GICC_CTLR_mask_t;

// GICC_PMR - Priority Mask Register
#define GICC_PMR_PRIORITY_MASK 0xFFu

// GICC_BPR - Binary Point Register
#define GICC_BPR_BINARYPOINT_MASK 0x7u

// GICC_IAR - Interrupt Acknowledge Register
#define GICC_IAR_INTID_MASK   0x3FFu       // Identifies the interrupt
#define GICC_IAR_CPUID_MASK   (0x7u << 10) // Identifies the source CPU for software-generated interrupts
#define GICC_IAR_CPUID_SHIFT  10u

#define GICC_IAR_SPURIOUS_ID  1023u // No eligible interrupt to acknowledge

// GICC_RPR - Current running interrupt priority
#define GICC_RPR_PRIORITY_MASK 0xFFu

// GICC_HPPIR - Highest-priority pending interrupt ID
#define GICC_HPPIR_INTID_MASK  0x3FFu
#define GICC_HPPIR_CPUID_MASK  (0x7u << 10)

//
// INTERRUPT IDS
//

typedef enum {
    GIC_IRQ_TIMER_VIRT    = 27,
    GIC_IRQ_TIMER_PHYS_NS = 30,
    GIC_IRQ_UART10        = 153,
} GIC_IRQ_t;

//
// PUBLIC FUNCTIONS
//

// Public initialization
void gic_init(void);

// Public initialization
void gic_enable_irq(GIC_IRQ_t irq);
void gic_disable_irq(GIC_IRQ_t irq);

// Configure interrupt priority and routing
void gic_set_priority(GIC_IRQ_t irq);
void gic_set_target(GIC_IRQ_t irq, uint8_t cpu_mask);

// Interrupt acknowledgement/completion
uint32_t gic_acknowledge_irq(void);
void gic_end_irq(uint32_t iar);
