#include "regs.h"
#include "acpi.h"
#include "serial_direct.h"

void entry(struct initial_regs* regs) {
	(void)regs;
	printf_serial_direct("powering off...\n");
	if (!acpiPoweroff()) {
		printf_serial_direct("failed!\n");
	} else {
		printf_serial_direct("success?????\n");
	}
}
