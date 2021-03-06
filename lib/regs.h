#ifndef PUSHA_REGS_H_GUARD_9EAAC03E_5F3A_45B3_A340_21D434F57508
#define PUSHA_REGS_H_GUARD_9EAAC03E_5F3A_45B3_A340_21D434F57508

struct pusha_regs {
	unsigned int edi, esi, ebp, esp, ebx, edx, ecx, eax;
};

struct initial_regs {
	struct pusha_regs iregs;
	unsigned int cr0, cr3, cr4, efer;
	unsigned int gdtr[3];
	unsigned int idtr[3];
	unsigned short cs, ds, ss, es, fs, gs;
};

#endif
