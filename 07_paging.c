#include "regs.h"
#include "serial_direct.h"

void entry(struct initial_regs* regs) {
	unsigned int esp, eip;

	printf_serial_direct("initial registers:\n");
	printf_serial_direct("EAX : 0x%08X\n", regs->iregs.eax);
	printf_serial_direct("EBX : 0x%08X\n", regs->iregs.ebx);
	printf_serial_direct("ECX : 0x%08X\n", regs->iregs.ecx);
	printf_serial_direct("EDX : 0x%08X\n", regs->iregs.edx);
	printf_serial_direct("ESI : 0x%08X\n", regs->iregs.esi);
	printf_serial_direct("EDI : 0x%08X\n", regs->iregs.edi);
	printf_serial_direct("ESP : 0x%08X\n", regs->iregs.esp);
	printf_serial_direct("EBP : 0x%08X\n", regs->iregs.ebp);
	printf_serial_direct("\n");
	printf_serial_direct("CR0 : 0x%08X\n", regs->cr0);
	printf_serial_direct("CR3 : 0x%08X\n", regs->cr3);
	printf_serial_direct("CR4 : 0x%08X\n", regs->cr4);
	printf_serial_direct("EFER: 0x%08X\n", regs->efer);

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
