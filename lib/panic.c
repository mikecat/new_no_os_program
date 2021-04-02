#include "panic.h"
#include "io_macro.h"
#include "serial_direct.h"

void panic(const char* message) {
	cli();
	putchar_serial_direct('p');
	putchar_serial_direct('a');
	putchar_serial_direct('n');
	putchar_serial_direct('i');
	putchar_serial_direct('c');
	putchar_serial_direct('!');
	if (message != 0) {
		int prev = 0;
		putchar_serial_direct(' ');
		putchar_serial_direct(':');
		putchar_serial_direct(' ');
		while (*message != '\0') {
			int current = (unsigned char)*message;
			if (current == '\n' && prev != '\r') {
				putchar_serial_direct('\r');
			}
			putchar_serial_direct(current);
			message++;
		}
	}
	putchar_serial_direct('\r');
	putchar_serial_direct('\n');
	for(;;) hlt();
}
