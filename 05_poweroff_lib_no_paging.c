#include "regs.h"
#include "acpi.h"
#include "serial_direct.h"

unsigned int simple_gdt[] = {
	0, 0,
	0x0000ffffu, 0x00cf9a00u, /* 0x08 code segment */
	0x0000ffffu, 0x00cf9200u  /* 0x10 data segment */
};

unsigned int guidTarget[] = {
	0x8868e871u, 0x11d3e4f1u, 0x800022bcu, 0x81883cc7u
};

void entry(void* placeholder, unsigned int* theTable) {
	unsigned int* altTable;
	int msr;

	unsigned int* guidTable;
	unsigned int guidTableNumElements;
	unsigned int guidTableElementSize;
	unsigned int i;

	__asm__ __volatile__ (
		"mov %%edx, %0\n\t"
		"mov $0xc0000080, %%ecx\n\t"
		"rdmsr\n\t"
		"mov %%eax, %1\n\t"
	: "=m"(altTable), "=m"(msr) : : "%ecx", "%eax", "%edx");

	(void)placeholder;
	if (msr & 0x400) {
		/* Long Mode */
		__asm__ __volatile__(
			"cli\n\t"

			"push %ebp\n\t"
			"mov %esp, %ebp\n\t"
			"sub $16, %esp\n\t"
			"movl $simple_gdt, 4(%esp)\n\t"
			"movl $0, 8(%esp)\n\t"
			"movw $23, 2(%esp)\n\t"
			"lea 2(%esp), %eax\n\t"

			/* far jump to switch to new CS */
			/* "ljmp $0x08, $cs_initialized\n\t" */
			"and $0x7ffffff0, %esp\n\t"
			"push $0x08\n\t"
			"push $cs_initialized\n\t"
			"lgdt (%eax)\n\t"
			"dec %eax\n\t"
			"retf\n\t"
			/* "ljmp *(%esp)\n\t" */
			"cs_initialized:\n\t"
			/* initialize data segments */
			"mov $0x10, %ax\n\t"
			"mov %ax, %ds\n\t"
			"mov %ax, %ss\n\t"
			"mov %ax, %es\n\t"
			"mov %ax, %fs\n\t"
			"mov %ax, %gs\n\t"
			"leave\n\t" /* mov %ebp, %esp ; pop %ebp */
			"pop %eax\n\t"

			/* disable paging for disabling Long Mode */
			"mov %cr0, %eax\n\t"
			"and $0x7fffffff, %eax\n\t"
			"mov %eax, %cr0\n\t"
			"jmp 1f\n\t" /* branch instruction after disabling paging */
			"1:\n\t"
		);

		guidTable = (unsigned int*)altTable[28];
		guidTableNumElements = altTable[26];
		guidTableElementSize = 6;
	} else {
		guidTable = (unsigned int*)theTable[17];
		guidTableNumElements = theTable[16];
		guidTableElementSize = 5;
	}
	init_serial_direct();
	for (i = 0; i < guidTableNumElements; i++) {
		unsigned int* element = guidTable + guidTableElementSize * i;
		if (element[0] == guidTarget[0] && element[1] == guidTarget[1] &&
		element[2] == guidTarget[2] && element[3] == guidTarget[3]) {
			if (!initAcpiWithRsdp((unsigned int*)element[4])) {
				printf_serial_direct("ACPI initialization failed!\n");
				for(;;);
			}
			break;
		}
	}
	if (i >= guidTableNumElements) {
		printf_serial_direct("RSDP not found!\n");
		for(;;);
	}

	if (!acpiPoweroff()) {
		printf_serial_direct("power off failed!\n");
	} else {
		printf_serial_direct("power off success?????\n");
	}
	for(;;);
}
