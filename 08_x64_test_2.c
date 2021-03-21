#include "serial_direct.h"

unsigned int gdt[] = {
	0, 0,
	0x0000ffffu, 0x00cf9a00u, /* 0x08 code segment */
	0x0000ffffu, 0x00cf9200u  /* 0x10 data segment */
};

void entry(void) {
	__asm__ __volatile__(
		"cli\n\t"

		"sub $16, %esp\n\t"
		"movl $gdt, 4(%esp)\n\t"
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
		"add $16, %esp\n\t"

		/* disable paging for disabling Long Mode */
		"mov %cr0, %eax\n\t"
		"and $0x7fffffff, %eax\n\t"
		"mov %eax, %cr0\n\t"
		"jmp 1f\n\t" /* branch instruction after disabling paging */
		"1:\n\t"
	);

	init_serial_direct();

	printf_serial_direct("hello, world\n");

	for(;;);
}
