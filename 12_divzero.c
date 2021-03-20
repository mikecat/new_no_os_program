#include "regs.h"
#include "serial_direct.h"
#include "interrupt.h"

void handler(struct interrupt_regs* regs) {
	(void)regs;
	printf_serial_direct("division by zero\n");
	for(;;);
}

void entry(struct initial_regs* regs) {
	int i, j;
	(void)regs;
	registerInterruptHandler(0, handler);
	for (i = 10; i >= 0; i--) {
		for (j = 0; j < 10 / i; j++) printf_serial_direct("*");
		printf_serial_direct("\n");
	}
}
