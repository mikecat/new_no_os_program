#include "regs.h"
#include "serial_direct.h"

void entry(struct initial_regs* regs) {
	static char gop_guid[] = {
		0xde,0xa9,0x42,0x90,0xdc,0x23,0x38,0x4a,
		0x96,0xfb,0x7a,0xde,0xd0,0x80,0x51,0x6a
	};

	unsigned int* arg_table = (unsigned int*)((unsigned int*)regs->iregs.esp)[2];
	unsigned int* services = (unsigned int*)arg_table[15];
	int (*LocateProtocol)(void*, void*, void*) =
		(int (*)(void*, void*, void*))services[6 + 37];
	unsigned int* gop = 0;
	int res;
	int i;

	res = LocateProtocol(gop_guid, 0, &gop);
	if (res < 0) {
		printf_serial_direct("LocateProtocol error 0x%08x\n", (unsigned int)res);
		return;
	}
	printf_serial_direct("gop = %p\n", (void*)gop);
	for (i = 0; i < 20; i++) {
		printf_serial_direct("0x%08x%c", gop[i], (i + 1) % 5 == 0 ? '\n' : ' ');
	}
}
