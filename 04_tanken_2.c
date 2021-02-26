#include "serial_direct.h"

void print_result(unsigned int addr, const int* results) {
	int i;
	printf_serial_direct("%08X :", addr);
	for (i = 0; i < 16; i++) {
		if (results[i] < 0) printf_serial_direct(" ??");
		else if (results[i] < 256) printf_serial_direct(" %02X", results[i]);
		else  printf_serial_direct("*%02X", results[i] - 256);
	}
	printf_serial_direct("\n");
}

void entry(void) {
	unsigned int esp, end, start;
	int i;
	int prev_results[16];
	unsigned int prev_result_addr = 0x100000 - 0x10000;
	for (i = 0; i < 16; i++) prev_results[i] = -256;
	__asm__ __volatile__("mov %%esp, %0\n\t" : "=g"(esp));
	init_serial_direct();
	end = (esp & 0xfff00000) + 0x100000;
	for (start = 0x100000; start < end; start += 0x10000) {
		int delta = 0;
		int current_results[16];
		int diff = 0;
		for (delta = 0; delta < 16; delta++) {
			short count[256] = {0};
			unsigned char* array = (unsigned char*)(start + delta * 0x1000);
			for (i = 0; i < 0x1000; i++) {
				count[array[i]]++;
			}
			current_results[delta] = -1;
			for (i = 0; i < 256; i++) {
				if (count[i] == 0x1000) {
					current_results[delta] = i;
					break;
				} else if (count[i] >= 0x1000 * 3 / 4) {
					current_results[delta] = i + 256;
					break;
				}
			}
		}
		for (i = 0; i < 16; i++) {
			if (prev_results[i] != current_results[i]) {
				diff = 1;
				break;
			}
		}
		if (diff || start + 0x10000 >= end) {
			if (prev_result_addr <= start - 0x100000) {
				printf_serial_direct("...\n");
			} else if (prev_result_addr < start - 0x10000) {
				unsigned int addr;
				for (addr = prev_result_addr + 0x10000; addr < start; addr += 0x10000) {
					print_result(addr, prev_results);
				}
			}
			print_result(start, current_results);
			prev_result_addr = start;
			for (i = 0; i < 16; i++) prev_results[i] = current_results[i];
		}
	}

	for(;;);
}
