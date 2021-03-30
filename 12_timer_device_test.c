#include "regs.h"
#include "serial_direct.h"
#include "interrupt.h"
#include "io_macro.h"

int count = 0;
int divider = 0;

void handler(struct interrupt_regs* regs) {
	(void)regs;
	divider++;
	if (divider >= 100) {
		divider = 0;
		count++;
		printf_serial_direct("%d\n", count);
	}
}

void entry(struct initial_regs* regs) {
	int counterSet = 3579545 / (3 * 100);
	(void)regs;
	registerInterruptHandler(0x20, handler);
	out8_imm8(0x34, 0x43); /* set timer 0 to pulse generation binary mode */
	out8_imm8(counterSet, 0x40);
	out8_imm8(counterSet >> 8, 0x40);
	setInterruptMask(0, 0);
	for(;;) {
		__asm__ __volatile__ ("hlt\n\t");
	}
}
