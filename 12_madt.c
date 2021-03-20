#include "regs.h"
#include "serial_direct.h"
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
		printf_serial_direct("MADT not found\n");
		return;
	}
	printf_serial_direct("Local APIC Address = 0x%08X\n", madt[9]);
	printf_serial_direct("Flags = 0x%08X\n", madt[10]);
	p = (unsigned char*)&madt[11];
	while ((p - (unsigned char*)madt) + 1 < (int)madt[1]) {
		printf_serial_direct("Type = %u\n", p[0]);
		printf_serial_direct("Length = %u\n", p[1]);
		switch (p[0]) {
			case 0:
				printf_serial_direct("Processor Local APIC structure\n");
				printf_serial_direct("ACPI Processor ID = 0x%02X\n", p[2]);
				printf_serial_direct("APIC ID = 0x%02X\n", p[3]);
				printf_serial_direct("Flags = 0x%08X\n", readNum(p + 4, 4));
				break;
			case 1:
				printf_serial_direct("I/O APIC structure\n");
				printf_serial_direct("I/O APIC ID = 0x%02X\n", p[2]);
				printf_serial_direct("I/O APIC Address = 0x%08X\n", readNum(p + 4, 4));
				printf_serial_direct("Global System Interrupt Base = 0x%08X\n", readNum(p + 8, 4));
				break;
			case 2:
				printf_serial_direct("Interrupt Source Override\n");
				printf_serial_direct("Bus = %d\n", p[2]);
				printf_serial_direct("Source = %d\n", p[3]);
				printf_serial_direct("Global System Interrupt = 0x%08X\n", readNum(p + 4, 4));
				printf_serial_direct("Flags = 0x%04X\n", readNum(p + 8, 2));
				break;
			case 3:
				printf_serial_direct("NMI\n");
				printf_serial_direct("Flags = 0x%04X\n", readNum(p + 2, 2));
				printf_serial_direct("Global System Interrupt = 0x%08X\n", readNum(p + 4, 4));
				break;
			case 4:
				printf_serial_direct("Local APIC NMI Structure\n");
				printf_serial_direct("ACPI Processor ID = 0x%02X\n", p[2]);
				printf_serial_direct("Flags = 0x%04X\n", readNum(p + 3, 2));
				printf_serial_direct("Local APIC LINT# = %d\n", p[5]);
				break;
			case 5:
				printf_serial_direct("Local APIC Address Override Structure\n");
				printf_serial_direct("Local APIC Address = 0x%08X%08X\n", readNum(p + 8, 4), readNum(p + 4, 4));
				break;
			case 6:
				printf_serial_direct("I/O SAPIC Structure\n");
				printf_serial_direct("I/O APIC ID = 0x%02X\n", p[2]);
				printf_serial_direct("Global System Interrupt Base = 0x%08X\n", readNum(p + 8, 4));
				printf_serial_direct("I/O SAPIC Address = 0x%08X%08X\n", readNum(p + 12, 4), readNum(p + 8, 4));
				break;
			case 7:
				printf_serial_direct("Processor Local SAPIC structure\n");
				printf_serial_direct("ACPI Processor ID = 0x%02X\n", p[2]);
				printf_serial_direct("Local SAPIC ID = 0x%02X\n", p[3]);
				printf_serial_direct("Local SAPIC EID = 0x%02X\n", p[4]);
				printf_serial_direct("Flags = 0x%08X\n", readNum(p + 8, 4));
				printf_serial_direct("ACPI Processor UID Value = 0x%08X\n", readNum(p + 12, 4));
				printf_serial_direct("ACPI Processor UID String = %s\n", (char*)(p + 16));
				break;
			case 8:
				printf_serial_direct("Flags = 0x%04X\n", readNum(p + 2, 2));
				printf_serial_direct("Interrupt Type = 0x%02X\n", p[4]);
				printf_serial_direct("Processor ID = 0x%02X\n", p[5]);
				printf_serial_direct("Processor EID = 0x%02X\n", p[6]);
				printf_serial_direct("I/O SAPIC Vector = 0x%02X\n", p[7]);
				printf_serial_direct("Global System Interrupt = 0x%08X\n", readNum(p + 8, 4));
				printf_serial_direct("Platform Interrupt Source Flags = 0x%08X\n", readNum(p + 12, 4));
				break;
		}
		printf_serial_direct("\n");
		p += p[1];
	}
}
