#include "gic.h"

// Distributor initialization
static void gicd_init(void) {
}

// CPU interface initialization
static void gicc_init(void) {
}

void gic_init(void) {

	// Enable Group 1 forwarding from distributor
	GICD->CTLR |= GICD_CTLR_ENABLEGRP1;

	// Allow all interrupt priorities through CPU interface
	GICC->PMR |= GICC_PMR_PRIORITY_MASK;

	// Enable Group 1 IRQ signaling to this CPU
	GICC->CTLR = (GICC->CTLR & ~GICC_CTLR_EOIMODENS) | GICC_CTLR_ENABLEGRP1;

	// Set priority
	gic_set_priority(GIC_IRQ_TIMER_PHYS_NS);

	// Enable interrupt ID 30
	gic_enable_irq(GIC_IRQ_TIMER_PHYS_NS);
}

void gic_enable_irq(GIC_IRQ_t irq) {
	GICD->ISENABLER[GICD_IRQ_REG(irq)] = GICD_IRQ_BIT(irq);
}

void gic_disable_irq(GIC_IRQ_t irq) {
	GICD->ICENABLER[GICD_IRQ_REG(irq)] = GICD_IRQ_BIT(irq);
}

void gic_set_priority(GIC_IRQ_t irq) {
	uint32_t index = GICD_PRIORITY_REG(irq);
	uint32_t value = GICD->IPRIORITYR[index];

	value &= ~GICD_PRIORITY_MASK(irq);
	value |= 0x80u << GICD_PRIORITY_SHIFT(irq);

	GICD->IPRIORITYR[index] = value;
}

void gic_set_target(GIC_IRQ_t irq, uint8_t cpu_mask) {
	// SGIs and PPIs do not use programmable SPI targeting
	if ((uint32_t)irq < 32) return;
	
	uint32_t index = GICD_TARGET_REG(irq);
	uint32_t value = GICD->ITARGETSR[index];

	value &= ~GICD_TARGET_MASK(irq);
	value |= (uint32_t)cpu_mask << GICD_TARGET_SHIFT(irq);

	GICD->ITARGETSR[index] = value;
}

uint32_t gic_acknowledge_irq(void) {
	return GICC->IAR;
}

void gic_end_irq(uint32_t iar) {
	GICC->EOIR = iar;
}

