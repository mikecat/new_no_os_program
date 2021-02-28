#include "initialize.h"
#include "regs.h"
#include "serial_direct.h"
#include "display.h"

void library_initialize(struct initial_regs* regs) {
	init_serial_direct();
	initializeDisplayInfo(regs);
}
