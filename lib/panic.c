#include "panic.h"
#include "io_macro.h"
#include "text_display.h"
#include "serial.h"

void panic(const char* message) {
	cli();
	textDisplayPanic();
	printfTextDisplay("panic!");
	if (message != 0) {
		printfTextDisplay(" : %s", message);
	}
	printfTextDisplay("\n");
	flushSerial();
	for(;;) hlt();
}
