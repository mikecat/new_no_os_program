#include "regs.h"
#include "uefi_printf.h"

void entry(struct initial_regs* regs) {
	(void)regs;

	uefiPrintf("hello, world\n");
	uefiPrintf("\n");
	uefiPrintf("[%d][%5d][%05d]\n", 1, 2, 3);
	uefiPrintf("[%d][%5d][%05d]\n", -1, -2, -3);
	uefiPrintf("[%x][%X][%o]\n", 0x12, 0x34, 0x56);
	uefiPrintf("[%5x][%5X][%5o]\n", 0x12, 0x34, 0x56);
	uefiPrintf("[%05x][%05X][%05o]\n", 0x12, 0x34, 0x56);
	uefiPrintf("[%s][%10s]\n", "hoge", "fuga");
	uefiPrintf("[%c][%5c]\n", 'a', 'b');
}
