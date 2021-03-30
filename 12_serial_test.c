#include "regs.h"
#include "serial_direct.h"
#include "interrupt.h"
#include "io_macro.h"

void handler(struct interrupt_regs* regs) {
	int stat, c;
	(void)regs;
	in8(stat, 0x03FA);
	printf_serial_direct("interrupt ID = 0x%02X\n", stat);
	in8(stat, 0x03FD);
	printf_serial_direct("data read =");
	while (stat & 1) { /* data available */
		in8(c, 0x03F8);
		printf_serial_direct(" %02X", c);
		in8(stat, 0x03FD);
	}
	printf_serial_direct("\n");
}

void entry(struct initial_regs* regs) {
	int stat;
	(void)regs;
	registerInterruptHandler(0x24, handler);
	out8(0x01, 0x3F9); /* enable Rx interrupt */
	in8(stat, 0x03FC);
	out8(stat | 0x08, 0x03FC); /* enable interrupt */
	setInterruptMask(4, 0);
	for(;;) {
		__asm__ __volatile__ ("hlt\n\t");
	}
}
