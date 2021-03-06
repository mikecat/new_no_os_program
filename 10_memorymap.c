#include "serial_direct.h"

unsigned int gdt[] = {
	0, 0,
	0x0000ffffu, 0x00cf9a00u, /* 0x08 code segment */
	0x0000ffffu, 0x00cf9200u  /* 0x10 data segment */
};

char data[4096 * 4];

void entry(void* placeholder, unsigned int* theTable) {
	unsigned int* altTable;
	int msr;

	int ret;
	int dataSize[2] = {sizeof(data), 0};
	int key[2] = {0, 0};
	int elementSize[2];
	int version[2];

	int i;

	__asm__ __volatile__ (
		"mov %%edx, %0\n\t"
		"mov $0xc0000080, %%ecx\n\t"
		"rdmsr\n\t"
		"mov %%eax, %1\n\t"
	: "=m"(altTable), "=m"(msr) : : "%ecx", "%eax", "%edx");

	(void)placeholder;
	if (msr & 0x400) {
		/* Long Mode */
		unsigned int* services = (unsigned int*)altTable[24];
		void* GetMemoryMap = (void*)services[6 + 2 * 4];
		int* pDataSize = dataSize, *pKey = key, *pElementSize = elementSize, *pVersion = version;
		__asm__ __volatile__ (
			"mov %3, %%ecx\n\t"
			"mov %4, %%edx\n\t"
			".byte 0x49, 0x89, 0xc8\n\t" /* mov %rcx, %r8 */
			".byte 0x49, 0x89, 0xd1\n\t" /* mov %rdx, $r9 */
			"mov %2, %%ecx\n\t"
			"mov $_data, %%edx\n\t"
			"mov %1, %%eax\n\t"
			"push %%ebp\n\t"
			"mov %%esp, %%ebp\n\t"
			"sub $40, %%esp\n\t"
			"and $0xfffffff0, %%esp\n\t"
			"mov %5, 32(%%esp)\n\t"
			"movl $0, 36(%%esp)\n\t"
			"call *%%eax\n\t"
			".byte 0x48, 0x85, 0xc0\n\t" /* test %rax, %rax */
			"jns 1f\n\t"
			"or $0x80000000, %%eax\n\t"
			"1:\n\t"
			"leave\n\t" /* mov %ebp, %esp ; pop %ebp */
			"mov %%eax, %0\n\t"
		: "=m"(ret) : "m"(GetMemoryMap), "m"(pDataSize), "m"(pKey), "m"(pElementSize), "r"(pVersion)
		: "%eax", "%ecx", "%edx", "cc", "memory");

		__asm__ __volatile__(
			"cli\n\t"

			"push %ebp\n\t"
			"mov %esp, %ebp\n\t"
			"sub $16, %esp\n\t"
			"movl $_gdt, 4(%esp)\n\t"
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
		unsigned int* services = (unsigned int*)theTable[15];
		int (*GetMemoryMap)(int*, void*, int*, int*, int*) =
			(int (*)(int*, void*, int*, int*, int*))services[6 + 4];
		ret = GetMemoryMap(dataSize, data, key, elementSize, version);
	}

	init_serial_direct();

	printf_serial_direct("GetMemoryMap() returned 0x%08X\n", ret);
	if (ret < 0) {
		printf_serial_direct("it seems something went wrong...\n");
		for(;;);
	}
	printf_serial_direct("data size    = %d\n", dataSize[0]);
	printf_serial_direct("element size = %d\n", elementSize[0]);
	printf_serial_direct("version      = %d\n", version[0]);
	printf_serial_direct("key          = 0x%08X%08X\n", key[1], key[0]);
	printf_serial_direct("\n");

	for (i = 0; i + elementSize[0] <= dataSize[0]; i += elementSize[0]) {
		int j;
		unsigned int *d = (unsigned int*)(data + i);
		for (j = 0; (j + 1) * (int)sizeof(*d) <= elementSize[0]; j++) {
			if (j > 0) printf_serial_direct(" ");
			printf_serial_direct("%08X", d[j]);
		}
		printf_serial_direct("\n");
	}

	for(;;);
}
