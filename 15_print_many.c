#include "regs.h"
#include "text_display.h"
#include "serial.h"

void entry(struct initial_regs* regs) {
	int i, j;
	(void)regs;
	for (i = 0; i < 2000; i += 10) {
		for (j = 0; j < 10; j++) {
			printfTextDisplay(" %4d", i + j);
		}
		printfTextDisplay("\n");
	}


	for (;;) {
		int c = serialGetchar();
		if (c == 'w' || c == 'W') { /* up */
			textDisplayMoveScroll(-5);
		} else if (c == 's' || c == 'S') { /* down */
			textDisplayMoveScroll(5);
		}
	}
}
