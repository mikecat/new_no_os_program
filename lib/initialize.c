#include "initialize.h"
#include "regs.h"
#include "serial_direct.h"
#include "display.h"
#include "acpi.h"
#include "uefi_printf.h"
#include "interrupt.h"

void library_initialize(struct initial_regs* regs) {
	init_serial_direct();
	initializeDisplayInfo(regs);
	initAcpi(regs);
	uefiPrintfInit(regs);
	initInterrupt(regs);
}
