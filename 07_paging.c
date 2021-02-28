#include "pusha_regs.h"
#include "serial_direct.h"

void entry(struct pusha_regs* regs) {
	unsigned int esp, eip;

	printf_serial_direct("initial registers:\n");
	printf_serial_direct("EAX : 0x%08X\n", regs->eax);
	printf_serial_direct("EBX : 0x%08X\n", regs->ebx);
	printf_serial_direct("ECX : 0x%08X\n", regs->ecx);
	printf_serial_direct("EDX : 0x%08X\n", regs->edx);
	printf_serial_direct("ESI : 0x%08X\n", regs->esi);
	printf_serial_direct("EDI : 0x%08X\n", regs->edi);
	printf_serial_direct("ESP : 0x%08X\n", regs->esp);
	printf_serial_direct("EBP : 0x%08X\n", regs->ebp);

	__asm__ __volatile__(
		"mov %%esp, %0\n\t"
		"call 1f\n\t"
		"1:\n\t"
		"pop %1\n\t"
	: "=r"(esp), "=r"(eip));
	printf_serial_direct("\ncurrent registers:\n");
	printf_serial_direct("ESP : 0x%08X\n", esp);
	printf_serial_direct("EIP : 0x%08X\n", eip);
}
