#include "initialize.h"
#include "pusha_regs.h"
#include "serial_direct.h"
#include "display.h"

void library_initialize(struct pusha_regs* regs) {
	init_serial_direct();
	initializeDisplayInfo(regs);
}
