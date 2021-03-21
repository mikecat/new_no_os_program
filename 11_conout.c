#include "serial_direct.h"

unsigned int gdt[] = {
	0, 0,
	0x0000ffffu, 0x00cf9a00u, /* 0x08 code segment */
	0x0000ffffu, 0x00cf9200u  /* 0x10 data segment */
};

char data[] = "h\0e\0l\0l\0o\0,\0 \0w\0o\0r\0l\0d\0\n\0\0\0";

void entry(void* placeholder, unsigned int* theTable) {
	unsigned int* altTable;
	int msr;

	int ret;

	__asm__ __volatile__ (
		"mov %%edx, %0\n\t"
		"mov $0xc0000080, %%ecx\n\t"
		"rdmsr\n\t"
		"mov %%eax, %1\n\t"
	: "=m"(altTable), "=m"(msr) : : "%ecx", "%eax", "%edx");

	(void)placeholder;
	if (msr & 0x400) {
		/* Long Mode */
		unsigned int* ConOut = (unsigned int*)altTable[16];
		void* OutputString = (void*)ConOut[2];
		char* pData = data;
		__asm__ __volatile__ (
			"mov %1, %%eax\n\t"
			"mov %2, %%ecx\n\t"
			"mov %3, %%edx\n\t"
			"push %%ebp\n\t"
			"mov %%esp, %%ebp\n\t"
			"sub $32, %%esp\n\t"
			"and $0xfffffff0, %%esp\n\t"
			"call *%%eax\n\t"
			".byte 0x48, 0x85, 0xc0\n\t" /* test %rax, %rax */
			"jns 1f\n\t"
			"or $0x80000000, %%eax\n\t"
			"1:\n\t"
			"leave\n\t" /* mov %ebp, %esp ; pop %ebp */
			"mov %%eax, %0\n\t"
		: "=m"(ret) : "m"(OutputString), "m"(ConOut), "m"(pData)
		: "%eax", "cc", "memory");

		__asm__ __volatile__(
			"cli\n\t"

			"push %ebp\n\t"
			"mov %esp, %ebp\n\t"
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
			"leave\n\t" /* mov %ebp, %esp ; pop %ebp */
			"pop %eax\n\t"

			/* disable paging for disabling Long Mode */
			"mov %cr0, %eax\n\t"
			"and $0x7fffffff, %eax\n\t"
			"mov %eax, %cr0\n\t"
			"jmp 1f\n\t" /* branch instruction after disabling paging */
			"1:\n\t"
		);
	} else {
		unsigned int* ConOut = (unsigned int*)theTable[11];
		int (*OutputString)(void*, void*) = (int(*)(void*, void*))ConOut[1];
		ret = OutputString(ConOut, data);
	}

	init_serial_direct();

	printf_serial_direct("OutputString() returned 0x%08X\n", ret);

	for(;;);
}
