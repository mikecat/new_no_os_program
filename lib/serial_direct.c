#include <stdarg.h>
#include "serial_direct.h"
#include "my_printf.h"
#include "io_macro.h"

void init_serial_direct(void) {
	/* set to 9600bps */
	out8(0x83, 0x03FB); /* enable LSB/MSB register */
	out8(0x00, 0x03F9); /* rate MSB */
	out8(0x0C, 0x03F8); /* rate LSB */
	/* set line info */
	out8(0x03, 0x03FB); /* disable LSB/MSB register, no parity, no 2-bit stop, 8-bit data */
	/* set moddem info */
	out8(0x03, 0x03FC); /* no loopback, no interrupt, RTS on, DTR on */
	/* enable FIFO */
	out8(0x07, 0x03FA); /* interrupt at 1 byte, clear TX, clear RX, enable */
	/* disable interrupt */
	out8(0x00, 0x03F9);
}

int putchar_serial_direct(int c) {
	int status;
	int data = (unsigned char)c;
	/* wait until TX buffer is empty */
	do {
		in8(status, 0x03FD);
	} while (!(status & 0x20));
	/* put data to send */
	out8(data, 0x03F8);
	return data;
}

int getchar_serial_direct(void) {
	int status, c;
	/* wait until data available */
	do {
		in8(status, 0x03FD);
	} while (!(status & 0x01));
	in8(c, 0x03F8);
	return c;
}

int getchar_serial_direct_nowait(void) {
	int status, c;
	/* check if data is available */
	in8(status, 0x03FD);
	if (!(status & 0x01)) return -1;
	in8(c, 0x03F8);
	return c;
}

static int printf_serial_direct_callback(const char* str, int str_len, void* status) {
	int i;
	(void)status;
	for (i = 0; i < str_len; i++) {
		if (str[i] == '\n' && (i == 0 || str[i - 1] != '\r') && putchar_serial_direct('\r') < 0) return 0;
		if (putchar_serial_direct(str[i]) < 0) return 0;
	}
	return 1;
}

int printf_serial_direct(const char* format, ...) {
	va_list vl;
	int ret;
	va_start(vl, format);
	ret = my_vprintf_base(printf_serial_direct_callback, 0, format, vl);
	va_end(vl);
	return ret;
}
