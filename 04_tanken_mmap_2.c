#include "serial_direct.h"

unsigned int dist(unsigned int a, unsigned int b) {
	return a < b ? b - a : a - b;
}

void entry(void) {
	unsigned int esp, end, start;
	__asm__ __volatile__("mov %%esp, %0\n\t" : "=g"(esp));
	init_serial_direct();
	end = (esp & 0xfff00000) + 0x100000;
	for (start = 0x500000; start < end; start += 4) {
		unsigned int* data = (unsigned int*)start;
		if (data[0] == 0x70616d6du && data[1] != 0 && dist(start, data[1]) < 0x2000 && dist(start, data[2]) >= 0x2000) {
			unsigned int list_start = data[2];
			unsigned int current = start + 4;
			int limitter = 0;
			printf_serial_direct("base %08X\n", list_start);
			while (current != list_start && limitter++ < 1000) {
				unsigned int* data2 = (unsigned int*)(current - 4);
				printf_serial_direct("%08X: %08X %08X | %08X %08X | %X %X %X %X %X %X %X %X %X\n",
					current - 4, data2[6], data2[8], data2[1], data2[2],
					data2[3], data2[4], data2[5], data2[7], data2[9], data2[10], data2[11], data2[12], data2[13]);
				current = data2[1];
			}
		}
	}

	for(;;);
}
