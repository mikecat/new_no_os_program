#ifndef SERIAL_DIRECT_H_GUARD_F2746C4B_617A_4EF4_BEBD_1DEF2DD5254B
#define SERIAL_DIRECT_H_GUARD_F2746C4B_617A_4EF4_BEBD_1DEF2DD5254B

#include <stdarg.h>

void serial_set_speed(int bps);
void init_serial_direct(void);
int putchar_serial_direct(int c);
int getchar_serial_direct(void);
int getchar_serial_direct_nowait(void);

int vprintf_serial_direct(const char* format, va_list args);
int printf_serial_direct(const char* format, ...);

#endif
