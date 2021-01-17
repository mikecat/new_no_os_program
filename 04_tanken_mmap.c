#include "serial_direct.h"

void entry(void) {
	unsigned int esp, end, start;
	__asm__ __volatile__("mov %%esp, %0\n\t" : "=g"(esp));
	end = (esp & 0xfff00000) + 0x100000;
	for (start = 0x500000; start < end; start += 4) {
		unsigned int* data = (unsigned int*)start;
		if (data[0] == 0x70616d6du) {
			printf_serial_direct("%08X: %08X %08X | %08X %08X | %X %X %X %X %X %X %X %X %X\n",
				start, data[6], data[8], data[1], data[2],
				data[3], data[4], data[5], data[7], data[9], data[10], data[11], data[12], data[13]);
		}
	}

	for(;;);
}
