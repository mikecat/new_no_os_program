#include "serial_direct.h"

void entry(void) {
	int i;
	init_serial_direct();
	putchar_serial_direct('h');
	putchar_serial_direct('e');
	putchar_serial_direct('l');
	putchar_serial_direct('l');
	putchar_serial_direct('o');
	putchar_serial_direct('\n');

	printf_serial_direct("hello, world\n");

	printf_serial_direct("\n");
	printf_serial_direct("[%d][%5d][%05d]\n", 1, 2, 3);
	printf_serial_direct("[%d][%5d][%05d]\n", -1, -2, -3);
	printf_serial_direct("[%x][%X][%o]\n", 0x12, 0x34, 0x56);
	printf_serial_direct("[%5x][%5X][%5o]\n", 0x12, 0x34, 0x56);
	printf_serial_direct("[%05x][%05X][%05o]\n", 0x12, 0x34, 0x56);
	printf_serial_direct("[%s][%10s]\n", "hoge", "fuga");
	printf_serial_direct("[%c][%5c]\n", 'a', 'b');

	for(;;);
}
