#include "serial_direct.h"

void entry(void) {
	__asm__ __volatile__(
		"cli\n\t"
		"mov %cr0, %eax\n\t"
		"and $0x7fffffff, %eax\n\t"
		"mov %eax, %cr0\n\t"
		"jmp 1f\n\t" /* instruction set changed, flash pipeline? */
		"1:\n\t"
	);

	init_serial_direct();

	printf_serial_direct("hello, world\n");

	for(;;);
}
