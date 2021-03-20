#include "interrupt.h"

/* interrupt_vector.S */
extern unsigned int interrupt_handler_list[];

static unsigned int interruptGate[2 * 256];

static interrupt_handler_type interruptHandlers[256];

int initInterrupt(struct initial_regs* regs) {
	int i;
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

	/* TODO: initialize 8259 or APIC */
	(void)regs;

	return 1;
}

void registerInterruptHandler(int id, interrupt_handler_type handler) {
	if (0 <= id && id < 256) interruptHandlers[id] = handler;
}

void interrupt_handler(struct interrupt_regs* regs) {
	if (0 <= regs->interruptId && regs->interruptId < 256 && interruptHandlers[regs->interruptId]) {
		interruptHandlers[regs->interruptId](regs);
	}
	/* TODO: send EOI if appropriate */
}
