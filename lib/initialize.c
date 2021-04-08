#include "initialize.h"
#include "regs.h"
#include "display.h"
#include "acpi.h"
#include "interrupt.h"
#include "text_display.h"
#include "serial.h"
#include "timer.h"

void library_initialize(struct initial_regs* regs) {
	initializeDisplayInfo(regs);
	initializeTextDisplay();
	initAcpi(regs);
	initInterrupt(regs);
	initSerial();
	initializeTimer();
}
