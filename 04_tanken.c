#include "serial_direct.h"

void entry(void) {
	unsigned int esp, end, start;
	__asm__ __volatile__("mov %%esp, %0\n\t" : "=g"(esp));
	end = (esp & 0xfff00000) + 0x100000;
	for (start = 0x100000; start < end; start += 0x1000) {
		short count[256] = {0};
		unsigned char* array = (unsigned char*)start;
		int found = -1, is_all = 0;
		int i;
		if ((start & 0xf000) == 0) {
			printf_serial_direct("\n%08X :", start);
		}
		for (i = 0; i < 0x1000; i++) {
			count[array[i]]++;
		}
		for (i = 0; i < 256; i++) {
			if (count[i] == 0x1000) {
				found = i;
				is_all = 1;
				break;
			} else if (count[i] >= 0x1000 * 3 / 4) {
				found = i;
				is_all = 0;
				break;
			}
		}
		if (found >= 0) {
			printf_serial_direct("%c%02X", is_all ? ' ' : '*', found);
		} else {
			printf_serial_direct(" ??");
		}
	}
	printf_serial_direct("\n");

	for(;;);
}
