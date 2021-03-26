#include "regs.h"
#include "uefi_printf.h"
#include "acpi.h"

unsigned int readNum(const unsigned char* p, int len) {
	int i;
	unsigned int ret = 0;
	for (i = 0; i < len; i++) {
		ret |= p[i] << (i * 8);
	}
	return ret;
}

void entry(struct initial_regs* regs) {
	unsigned int* madt = getAcpiTable("APIC", 0);
	unsigned char* p;
	(void)regs;
	if (madt == 0) {
		uefiPrintf("MADT not found\n");
		return;
	}
	uefiPrintf("Local APIC Address = 0x%08X\n", madt[9]);
	uefiPrintf("Flags = 0x%08X\n", madt[10]);
	p = (unsigned char*)&madt[11];
	while ((p - (unsigned char*)madt) + 1 < (int)madt[1]) {
		uefiPrintf("Type = %u\n", p[0]);
		uefiPrintf("Length = %u\n", p[1]);
		switch (p[0]) {
			case 0:
				uefiPrintf("Processor Local APIC structure\n");
				uefiPrintf("ACPI Processor ID = 0x%02X\n", p[2]);
				uefiPrintf("APIC ID = 0x%02X\n", p[3]);
				uefiPrintf("Flags = 0x%08X\n", readNum(p + 4, 4));
				break;
			case 1:
				uefiPrintf("I/O APIC structure\n");
				uefiPrintf("I/O APIC ID = 0x%02X\n", p[2]);
				uefiPrintf("I/O APIC Address = 0x%08X\n", readNum(p + 4, 4));
				uefiPrintf("Global System Interrupt Base = 0x%08X\n", readNum(p + 8, 4));
				break;
			case 2:
				uefiPrintf("Interrupt Source Override\n");
				uefiPrintf("Bus = %d\n", p[2]);
				uefiPrintf("Source = %d\n", p[3]);
				uefiPrintf("Global System Interrupt = 0x%08X\n", readNum(p + 4, 4));
				uefiPrintf("Flags = 0x%04X\n", readNum(p + 8, 2));
				break;
			case 3:
				uefiPrintf("NMI\n");
				uefiPrintf("Flags = 0x%04X\n", readNum(p + 2, 2));
				uefiPrintf("Global System Interrupt = 0x%08X\n", readNum(p + 4, 4));
				break;
			case 4:
				uefiPrintf("Local APIC NMI Structure\n");
				uefiPrintf("ACPI Processor ID = 0x%02X\n", p[2]);
				uefiPrintf("Flags = 0x%04X\n", readNum(p + 3, 2));
				uefiPrintf("Local APIC LINT# = %d\n", p[5]);
				break;
			case 5:
				uefiPrintf("Local APIC Address Override Structure\n");
				uefiPrintf("Local APIC Address = 0x%08X%08X\n", readNum(p + 8, 4), readNum(p + 4, 4));
				break;
			case 6:
				uefiPrintf("I/O SAPIC Structure\n");
				uefiPrintf("I/O APIC ID = 0x%02X\n", p[2]);
				uefiPrintf("Global System Interrupt Base = 0x%08X\n", readNum(p + 8, 4));
				uefiPrintf("I/O SAPIC Address = 0x%08X%08X\n", readNum(p + 12, 4), readNum(p + 8, 4));
				break;
			case 7:
				uefiPrintf("Processor Local SAPIC structure\n");
				uefiPrintf("ACPI Processor ID = 0x%02X\n", p[2]);
				uefiPrintf("Local SAPIC ID = 0x%02X\n", p[3]);
				uefiPrintf("Local SAPIC EID = 0x%02X\n", p[4]);
				uefiPrintf("Flags = 0x%08X\n", readNum(p + 8, 4));
				uefiPrintf("ACPI Processor UID Value = 0x%08X\n", readNum(p + 12, 4));
				uefiPrintf("ACPI Processor UID String = %s\n", (char*)(p + 16));
				break;
			case 8:
				uefiPrintf("Flags = 0x%04X\n", readNum(p + 2, 2));
				uefiPrintf("Interrupt Type = 0x%02X\n", p[4]);
				uefiPrintf("Processor ID = 0x%02X\n", p[5]);
				uefiPrintf("Processor EID = 0x%02X\n", p[6]);
				uefiPrintf("I/O SAPIC Vector = 0x%02X\n", p[7]);
				uefiPrintf("Global System Interrupt = 0x%08X\n", readNum(p + 8, 4));
				uefiPrintf("Platform Interrupt Source Flags = 0x%08X\n", readNum(p + 12, 4));
				break;
		}
		uefiPrintf("\n");
		p += p[1];
	}
}
