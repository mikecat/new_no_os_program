#include "interrupt.h"
#include "acpi.h"
#include "io_macro.h"

/* interrupt_vector.S */
extern unsigned int interrupt_handler_list[];

static unsigned int interruptGate[2 * 256];

static interrupt_handler_type interruptHandlers[256];

int initInterrupt(struct initial_regs* regs) {
	int i;
	unsigned int* madt;

	(void)regs;

	/* initialize IDT */
	for (i = 0; i < 256; i++) {
		interruptGate[i * 2 + 0] = (interrupt_handler_list[i] & 0xffff) | (0x08 << 16);
		interruptGate[i * 2 + 1] = (interrupt_handler_list[i] & 0xffff0000u) | 0x8e00;
	}
	__asm__ __volatile__ (
		"sub $8, %%esp\n\t"
		"mov %%ax, 2(%%esp)\n\t"
		"mov %0, 4(%%esp)\n\t"
		"lidt 2(%%esp)\n\t"
		"add $8, %%esp\n\t"
	: : "r"(interruptGate), "a"(sizeof(interruptGate)) : "memory");

	madt = getAcpiTable("MADT", 0);
	/* initialize 8259 */
	if (madt == 0 || (madt[10] & 1)) {
		/* ICW1 */
		out8_imm8(0x11, 0x20);
		out8_imm8(0x11, 0xA0);
		/* ICW2 */
		out8_imm8(0x20, 0x21); /* master: IRQ 0 - 7  -> vector 0x20 - 0x27 */
		out8_imm8(0x28, 0xA1); /* slave : IRQ 8 - 15 -> vector 0x28 - 0x2F */
		/* ICW3 */
		out8_imm8(0x04, 0x21);
		out8_imm8(0x02, 0xA1);
		/* ICW4 */
		out8_imm8(0x01, 0x21);
		out8_imm8(0x01, 0xA1);
		/* OCW1 (interrupt mask) */
		out8_imm8(0xff, 0x21);
		out8_imm8(0xff, 0xA1);
	}

	/* enable slave -> master interrupt */
	out8_imm8(0xfb, 0x21);

	sti();
	return 1;
}

void setInterruptMask(int irq, int mask) {
	int data;
	if (0 <= irq && irq < 8) {
		in8_imm8(data, 0x21);
		out8_imm8(mask ? data | (1 << irq) : data & ~(1 << irq), 0x21);
	} else if (8 <= irq && irq < 16) {
		in8_imm8(data, 0xA1);
		out8_imm8(mask ? data | (1 << (irq - 8)) : data & ~(1 << (irq - 8)), 0xA1);
	}
}

void registerInterruptHandler(int id, interrupt_handler_type handler) {
	if (0 <= id && id < 256) interruptHandlers[id] = handler;
}

void interrupt_handler(struct interrupt_regs* regs) {
	if (0 <= regs->interruptId && regs->interruptId < 256 && interruptHandlers[regs->interruptId]) {
		interruptHandlers[regs->interruptId](regs);
	}
	if (0x20 <= regs->interruptId && regs->interruptId < 0x30) {
		out8_imm8(0x20, 0x20); /* EOI to master */
	}
	if (0x28 <= regs->interruptId && regs->interruptId < 0x30) {
		out8_imm8(0x20, 0xA0); /* EOI to slave */
	}
}
