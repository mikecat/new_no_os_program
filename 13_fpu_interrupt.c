#include "regs.h"
#include "serial_direct.h"
#include "interrupt.h"
#include "io_macro.h"

int divider;

volatile int currentIter;
volatile double currentValue;

void handler(struct interrupt_regs* regs) {
	(void)regs;
	divider++;
	if (divider >= 100) {
		double displayValue = currentValue * 4;
		divider = 0;
		printf_serial_direct("%08X ", regs->eip);
		printf_serial_direct("%10d : %d.", currentIter, (int)displayValue);
		displayValue -= (int)displayValue;
		displayValue *= 10000000;
		printf_serial_direct("%07d", (int)displayValue);
		displayValue -= (int)displayValue;
		displayValue *= 10000000;
		printf_serial_direct("%07d\n", (int)displayValue);
	}
}

void entry(struct initial_regs* regs) {
	int counterSet = 3579545 / (3 * 100);
	double calculationValue = 0;
	(void)regs;
	registerInterruptHandler(0x20, handler);
	divider = 0;
	out8_imm8(0x34, 0x43); /* set timer 0 to pulse generation binary mode */
	out8_imm8(counterSet, 0x40);
	out8_imm8(counterSet >> 8, 0x40);
	setInterruptMask(0, 0);
	for (currentIter = 0; currentIter < 1000000000; ) {
		double delta = 1.0 / (currentIter * 2.0 + 1);
		if (currentIter % 2 != 0) delta = -delta;
		calculationValue += delta;
		currentValue = calculationValue;
		currentIter++;
	}
	for(;;) hlt();
}
