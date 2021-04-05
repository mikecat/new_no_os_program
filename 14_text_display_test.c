#include "regs.h"
#include "text_display.h"
#include "serial_direct.h"

void entry(struct initial_regs* regs) {
	int escStat = 0;
	int escNum = 0;
	(void)regs;
	for (;;) {
		int c = getchar_serial_direct();
		if (c == 0x1b) {
			escStat = 1;
			escNum = 0;
		} else if (escStat == 1) {
			if (c == '[') {
				escStat = 2;
			} else {
				escStat = 0;
			}
		} else if (escStat == 2) {
			if ('0' <= c && c <= '9') {
				escNum = c - '0';
				escStat = 3;
			} else {
				if (c == 'A') { /* up arrow */
					textDisplayMoveScroll(-5);
				} else if (c == 'B') { /* down arrow */
					textDisplayMoveScroll(5);
				}
				escStat = 0;
			}
		} else if (escStat == 3) {
			if ('0' <= c && c <= '9') {
				escNum = escNum * 10 + (c - '0');
			} else if (c == '~') {
				switch (escNum) {
					case 1: /* Home */
						textDisplaySetScroll(0);
						break;
					case 4: /* End */
						textDisplaySetScroll(textDisplayGetScrollMax());
						break;
					case 5: /* Page UP */
						textDisplayMoveScroll(-(textDisplayGetDisplayLineNum() - 1));
						break;
					case 6: /* Page Down */
						textDisplayMoveScroll(textDisplayGetDisplayLineNum() - 1);
						break;
				}
				escStat = 0;
			}
		} else {
			printfTextDisplay("%c", c);
		}
	}
}
