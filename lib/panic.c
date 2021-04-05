#include "panic.h"
#include "io_macro.h"
#include "text_display.h"

void panic(const char* message) {
	cli();
	textDisplayPanic();
	printfTextDisplay("panic!");
	if (message != 0) {
		printfTextDisplay(" : %s", message);
	}
	printfTextDisplay("\n");
	for(;;) hlt();
}
