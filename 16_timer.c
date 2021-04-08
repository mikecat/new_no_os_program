#include "regs.h"
#include "text_display.h"
#include "serial.h"
#include "timer.h"

struct timer_set_data {
	const char* message;
	unsigned int duration;
};

void timer_callback(void* data) {
	struct timer_set_data* tsd = data;
	printfTextDisplay("%s (tick = %u)\n", tsd->message, getTimerTick());
	setTimer(tsd->duration, timer_callback, data);
}

void entry(struct initial_regs* regs) {
	struct timer_set_data tsd[] = {
		{"3s timer!", 3000},
		{"5s timer!", 5000},
		{"7s timer!", 7000},
	};
	int i;
	(void)regs;
	for (i = 0; i < (int)(sizeof(tsd) / sizeof(*tsd)); i++) {
		setTimer(tsd[i].duration, timer_callback, &tsd[i]);
	}

	for (;;) {
		int c = serialGetchar();
		if (c == 'w' || c == 'W') { /* up */
			textDisplayMoveScroll(-5);
		} else if (c == 's' || c == 'S') { /* down */
			textDisplayMoveScroll(5);
		} else if (c == 't' || c == 'T') {
			printfTextDisplay("tick = %u\n", getTimerTick());
		}
	}
}
