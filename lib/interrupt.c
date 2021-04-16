#include "interrupt.h"
#include "acpi.h"
#include "pages.h"
#include "io_macro.h"
#include "text_display.h"
#include "memory_utils.h"

/* interrupt_vector.S */
extern unsigned int interrupt_handler_list[];

static unsigned int interruptGate[2 * 256];

static interrupt_handler_type interruptHandlers[256];

struct ioapic_info {
	unsigned int physicalAddr;
	volatile unsigned int* logicalAddr;
	unsigned int ibase;
	unsigned char inum, is_sapic;
};

static struct ioapic_info ioapics[256];

volatile unsigned int* localApic;

enum {
	Pol_activeHigh = 1, Pol_activeLow = 3,
	Tri_edge = 1, Tri_level = 3
};

struct interruptSourceOverride_info {
	unsigned int globalSystemInterrupt;
	unsigned char pol, tri, isDefault, disabled;
	unsigned char ioApicId, ioApicOffset;
};

static struct interruptSourceOverride_info interruptSourceOverride[16];

enum {
	INTERRUPT_UNINITIALIZED,
	INTERRUPT_UNSUPPORTED,
	INTERRUPT_8259,
	INTERRUPT_xAPIC,
	INTERRUPT_x2APIC
};

static int interruptMode = INTERRUPT_UNINITIALIZED;

enum {
	FPU_NONE = 0,
	FPU_x87,
	FPU_SSE,
	FPU_AVX
};

int fpuSaveMode = FPU_NONE;
int fpuSaveSize, fpuSaveAlignmentSize;

enum {
	FPU_SAVE_NO = 0,
	FPU_SAVE_MAYBE = 1,
	FPU_SAVE_YES = 2
};

int* delayedFpuSavePtr;

static unsigned int read2(const unsigned char* data) {
	return data[0] | (data[1] << 8);
}

static unsigned int read4(const unsigned char* data) {
	return data[0] | (data[1] << 8) | (data[2] << 16) | (data[3] << 24);
}

int initInterrupt(struct initial_regs* regs) {
	unsigned int i;
	unsigned int* madt;
	int simdAvailability;
	unsigned int cpuid_edx, cpuid_ecx;
	int is_8259_available = 0, apic_available = 0, x2apic_available = 0;
	if (interruptMode != INTERRUPT_UNINITIALIZED) return interruptMode != INTERRUPT_UNSUPPORTED;

	(void)regs;

	/* initialize IDT */
	for (i = 0; i < 256; i++) {
		interruptGate[i * 2 + 0] = (interrupt_handler_list[i] & 0xffff) | (0x08 << 16);
		interruptGate[i * 2 + 1] = (interrupt_handler_list[i] & 0xffff0000u) | 0x8e00;
	}
	__asm__ __volatile__ (
		"sub $8, %%esp\n\t"
		"mov %%ax, 2(%%esp)\n\t"
		"mov %0, 4(%%esp)\n\t"
		"lidt 2(%%esp)\n\t"
		"add $8, %%esp\n\t"
	: : "r"(interruptGate), "a"(sizeof(interruptGate)) : "memory");

	/* enable SSE and AVX register saving if available */
	simdAvailability = checkSimdAvailability();
	delayedFpuSavePtr = 0;
	if (simdAvailability & SIMD_AVAILABILITY_AVX) {
		/* AVX (XSAVE) support */
		fpuSaveMode = FPU_AVX;
		fpuSaveAlignmentSize = 64;
		__asm__ __volatile__ (
			"mov $0x0d, %%eax\n\t"
			"xor %%ecx, %%ecx\n\t"
			"cpuid\n\t"
		: "=b"(fpuSaveSize) : : "%eax", "%ecx", "%edx");
	} else if (simdAvailability & SIMD_AVAILABILITY_SSE) {
		/* SSE (FXSAVE) support */
		fpuSaveMode = FPU_SSE;
		fpuSaveSize = 512;
		fpuSaveAlignmentSize = 16;
	} else if (simdAvailability & SIMD_AVAILABILITY_FPU) {
		/* basic FPU support */
		fpuSaveMode = FPU_x87;
		fpuSaveSize = 108;
		fpuSaveAlignmentSize = 4;
	} else {
		fpuSaveMode = FPU_NONE;
	}
	/* set TS = 1 to avoid unneeded allocation in non-FPU-using program */
	if (fpuSaveMode != FPU_NONE) {
		__asm__ __volatile__ (
			"mov %%cr0, %%eax\n\t"
			"or $8, %%eax\n\t"
			"mov %%eax, %%cr0\n\t"
		: : : "%eax");
	}

	madt = getAcpiTable("APIC", 0);
	/* initialize 8259 */
	if (madt == 0 || (madt[10] & 1)) {
		is_8259_available = 1;
		/* ICW1 */
		out8_imm8(0x11, 0x20);
		out8_imm8(0x11, 0xA0);
		/* ICW2 */
		out8_imm8(0x20, 0x21); /* master: IRQ 0 - 7  -> vector 0x20 - 0x27 */
		out8_imm8(0x28, 0xA1); /* slave : IRQ 8 - 15 -> vector 0x28 - 0x2F */
		/* ICW3 */
		out8_imm8(0x04, 0x21);
		out8_imm8(0x02, 0xA1);
		/* ICW4 */
		out8_imm8(0x01, 0x21);
		out8_imm8(0x01, 0xA1);
		/* OCW1 (interrupt mask) */
		out8_imm8(0xff, 0x21);
		out8_imm8(0xff, 0xA1);
	}
	/* initialize APIC */
	__asm__ __volatile__ (
		"xor %%eax, %%eax\n\t"
		"cpuid\n\t"
		"test %%eax, %%eax\n\t"
		"jnz 2f\n\t"
		/* CPUID with eax=1 not supported */
		"xor %%ecx, %%ecx\n\t"
		"xor %%edx, %%edx\n\t"
		"jmp 1f\n\t"
		"2:\n\t"
		"mov $1, %%eax\n\t"
		"cpuid\n\t"
		"1:\n\t"
	: "=d"(cpuid_edx), "=c"(cpuid_ecx) : : "%eax", "%ebx");
	if ((cpuid_edx & 0x200) && madt != 0 && madt[1] >= 46) {
		unsigned int lapic = madt[9], lapicHigh = 0;
		unsigned char* tableBegin = (unsigned char*)&madt[11];
		unsigned char* table = tableBegin;
		unsigned int tableSize = madt[1] - 44;
		int invalid = 0;
		unsigned int apicId = 0;
		unsigned int* cr3;
		get_cr3(cr3);
		/* retrieve Local APIC address and I/O APIC information */
		for (i = 0; i < 256; i++) {
			ioapics[i].logicalAddr = 0;
			ioapics[i].is_sapic = 0;
		}
		for (i = 0; i < 16; i++) {
			interruptSourceOverride[i].globalSystemInterrupt = i;
			interruptSourceOverride[i].pol = Pol_activeHigh;
			interruptSourceOverride[i].tri = Tri_edge;
			interruptSourceOverride[i].isDefault = 1;
			interruptSourceOverride[i].disabled = 0;
		}
		while ((unsigned int)(table - tableBegin) <= tableSize - 2) {
			unsigned int type = table[0], length = table[1];
			if (length <= tableSize && (unsigned int)(table - tableBegin) <= tableSize - length) {
				switch (type) {
					case 1: /* I/O APIC Structure */
						if (length >= 12) {
							int id = table[2];
							if (ioapics[id].logicalAddr == 0) {
								ioapics[id].physicalAddr = read4(&table[4]);
								ioapics[id].logicalAddr = (unsigned int*)0xffffffffu;
								ioapics[id].ibase = read4(&table[8]);
							} else if (!ioapics[id].is_sapic) {
								printfTextDisplay("interrupt: multiple I/O APIC id %d\n", id);
								invalid = 1;
							}
						}
						break;
					case 2: /* Interrupt Source Override Structure */
						if (length >= 10) {
							int id = table[3];
							int flags = read2(&table[8]);
							if (0 <= id && id < 16) {
								interruptSourceOverride[id].globalSystemInterrupt = read4(&table[4]);
								if ((flags & 3) != 0) interruptSourceOverride[id].pol = flags & 3;
								if (((flags >> 2) & 3) != 0) interruptSourceOverride[id].tri = (flags >> 2) & 3;
								interruptSourceOverride[id].isDefault = 0;
							}
						}
						break;
					case 5: /* Local APIC Address Override Structure */
						if (length >= 12) {
							lapic = read4(&table[4]);
							lapicHigh = read4(&table[8]);
						}
						break;
					case 6: /* I/O SAPIC Structure */
						if (length >= 16) {
							int id = table[2];
							if (ioapics[id].logicalAddr == 0 || !ioapics[id].is_sapic) {
								ioapics[id].physicalAddr = read4(&table[8]);
								ioapics[id].logicalAddr = (unsigned int*)0xffffffffu;
								ioapics[id].ibase = read4(&table[4]);
								if (read4(&table[12]) != 0) {
									printfTextDisplay("interrupt: high I/O SAPIC address not supported\n");
									invalid = 1;
								}
							} else {
								printfTextDisplay("interrupt: multiple I/O SAPIC id %d\n", id);
								invalid = 1;
							}
						}
						break;
				}
			}
			table += length;
		}
		if (!invalid) {
			/* initialize I/O APIC map and retrieve information */
			for (i = 0; i < 256; i++) {
				if (ioapics[i].logicalAddr) {
					unsigned int addr = allocate_region_without_allocation(4096);
					ioapics[i].logicalAddr = (unsigned int*)addr;
					map_pde(cr3, addr, ioapics[i].physicalAddr, PAGE_FLAG_CACHE_DISABLE | PAGE_FLAG_WRITE | PAGE_FLAG_PRESENT);
					ioapics[i].logicalAddr[0] = 0x01;
					ioapics[i].inum = ((ioapics[i].logicalAddr[4] >> 16) & 0xff) + 1;
				}
			}
			for (i = 0; i < 16; i++) {
				unsigned int j;
				/* skip if already disabled by collision check */
				if (interruptSourceOverride[i].disabled) continue;
				interruptSourceOverride[i].disabled = 1;
				for (j = 0; j < 256; j++) {
					if (ioapics[j].logicalAddr && ioapics[j].ibase <= interruptSourceOverride[i].globalSystemInterrupt &&
					interruptSourceOverride[i].globalSystemInterrupt - ioapics[j].ibase < ioapics[j].inum) {
						interruptSourceOverride[i].ioApicId = j;
						interruptSourceOverride[i].ioApicOffset = (unsigned char)(interruptSourceOverride[i].globalSystemInterrupt - ioapics[j].ibase);
						interruptSourceOverride[i].disabled = 0;
						break;
					}
				}
				/* skip if appropriate I/O APIC is not found */
				if (interruptSourceOverride[i].disabled) continue;
				/* collision check */
				for (j = i + 1; j < 16; j++) {
					if (interruptSourceOverride[j].disabled) continue;
					if (interruptSourceOverride[i].globalSystemInterrupt == interruptSourceOverride[j].globalSystemInterrupt) {
						if (interruptSourceOverride[i].isDefault != interruptSourceOverride[j].isDefault) {
							if (interruptSourceOverride[i].isDefault) interruptSourceOverride[i].disabled = 1;
							else interruptSourceOverride[j].disabled = 1;
						} else {
							printfTextDisplay("interrupt: interrupt collision for IRQ %u and %u\n", i, j);
							invalid = 1;
						}
					}
				}
			}
			/* MSR exists and x2APIC exists */
			x2apic_available = (cpuid_edx & 0x20) && (cpuid_ecx & 0x200000);
			if (x2apic_available) {
				/* enable x2APIC */
				__asm__ __volatile__ (
					"mov $0x1b, %%edx\n\t"
					"rdmsr\n\t"
					"or $0x400, %%eax\n\t"
					"wrmsr\n\t"
				: : : "%eax", "%ecx", "%edx");
				/* TPR */
				write_msr32(0x00, 0x808);
				/* Sprious Interrupt = 0xff, Disabled */
				write_msr32(0xff, 0x80f);
				/* LVT Timer */
				write_msr32(0x10030, 0x832);
				/* LVT Thermal Sensor */
				write_msr32(0x10031, 0x833);
				/* LVT Performance Monitoring */
				write_msr32(0x10032, 0x834);
				/* LVT LINT0 */
				write_msr32(0x10033, 0x835);
				/* LVT LINT1 */
				write_msr32(0x10034, 0x836);
				/* LVT Error */
				write_msr32(0x10035, 0x837);
#if 0
				/* LVT CMCI */
				write_msr32(0x10036, 0x82f);
#endif
				/* APIC id */
				read_msr32(apicId, 0x802);
			} else if (lapicHigh != 0) {
				printfTextDisplay("interrupt: High Local APIC address not supported\n");
				invalid = 1;
			} else {
				/* initialize xAPIC */
				unsigned int localApicAddr = allocate_region_without_allocation(4096);
				localApic = (unsigned int*)localApicAddr;
				map_pde(cr3, localApicAddr, lapic, PAGE_FLAG_CACHE_DISABLE | PAGE_FLAG_WRITE | PAGE_FLAG_PRESENT);
				/* TPR */
				localApic[0x80 >> 2] = 0x00;
				/* Sprious Interrupt = 0xff, Disabled */
				localApic[0xf0 >> 2] = 0xff;
				/* LVT Timer */
				localApic[0x320 >> 2] = 0x10030;
				/* LVT Thermal Sensor */
				localApic[0x330 >> 2] = 0x10031;
				/* LVT Performance Monitoring */
				localApic[0x340 >> 2] = 0x10032;
				/* LVT LINT0 */
				localApic[0x350 >> 2] = 0x10033;
				/* LVT LINT1 */
				localApic[0x360 >> 2] = 0x10034;
				/* LVT Error */
				localApic[0x370 >> 2] = 0x10035;
#if 0
				/* LVT CMCI */
				localApic[0x2f0 >> 2] = 0x10036;
#endif
				/* APIC id */
				apicId = localApic[0x20 >> 2];
			}
		}
		if (!invalid) {
			unsigned int processorId = 0, processorIdValid = 0;
			/* initialize I/O APICs */
			for (i = 0; i < 256; i++) {
				if (ioapics[i].logicalAddr) {
					int j;
					for (j = 0; j < ioapics[i].inum; j++) {
						/* masked, edge trigger, high active, physical mode, fixed, vector = 0xe0 */
						ioapics[i].logicalAddr[0] = 0x10 + 2 * j;
						ioapics[i].logicalAddr[4] = 0x100e0;
						/* destination = broadcast */
						ioapics[i].logicalAddr[0] = 0x10 + 2 * j + 1;
						ioapics[i].logicalAddr[4] = 0xff000000u;
					}
				}
			}
			/* determine processor ID from APIC ID */
			table = tableBegin;
			while ((unsigned int)(table - tableBegin) <= tableSize - 2) {
				unsigned int type = table[0], length = table[1];
				if (length <= tableSize && (unsigned int)(table - tableBegin) <= tableSize - length) {
					switch (type) {
						case 0: /* Processor Local APIC Structure */
							if (length >= 8) {
								if (table[4] & 1) { /* Enabled */
									if (table[3] == (apicId >> 24)) {
										processorId = table[2];
										processorIdValid = 1;
									}
								}
							}
							break;
						case 9: /* Processor Local x2APIC Structure */
							if (length >= 16) {
								if (table[8] & 1) { /* Enabled */
									if (read4(&table[4]) == apicId) {
										processorId = read4(&table[12]);
										processorIdValid = 1;
									}
								}
							}
							break;
					}
				}
				table += length;
			}
			/* enable local APIC */
			if (x2apic_available) {
				write_msr32(0x1ff, 0x80f);
			} else {
				localApic[0xf0 >> 2] = 0x1ff;
			}
			/* configure NMI */
			table = tableBegin;
			while ((unsigned int)(table - tableBegin) <= tableSize - 2) {
				unsigned int type = table[0], length = table[1];
				if (length <= tableSize && (unsigned int)(table - tableBegin) <= tableSize - length) {
					switch (type) {
						case 3: /* Non-maskable Interrupt Source Structure */
							if (length >= 8) {
								int flags = read2(&table[2]);
								unsigned int interrupt = read4(&table[4]);
								int pol = flags & 3, tri = (flags >> 2) & 3;
								for (i = 0; i < 256; i++) {
									if (ioapics[i].logicalAddr && ioapics[i].ibase <= interrupt &&
									interrupt - ioapics[i].ibase < ioapics[i].inum) {
										unsigned int value = 0x00400;
										if (pol == Pol_activeLow) value |= 0x2000;
										if (tri == Tri_level) value |= 0x8000;
										ioapics[i].logicalAddr[0] = 0x10 + 2 * (interrupt - ioapics[i].ibase);
										ioapics[i].logicalAddr[4] = value;
										break;
									}
								}
							}
							break;
						case 4: /* Local APIC NMI Structure */
							if (length >= 6) {
								if (table[2] == 0xff || (processorIdValid && table[2] == processorId)) {
									int lint = table[5];
									if (lint == 0 || lint == 1) {
										int flags = read2(&table[3]);
										int pol = flags & 3, tri = (flags >> 2) & 3;
										unsigned int value = 0x00400;
										if (pol == Pol_activeLow) value |= 0x2000;
										if (tri == Tri_level) value |= 0x8000;
										if (x2apic_available) {
											write_msr32(value, 0x835 + lint);
										} else {
											localApic[(0x350 + 0x10 * lint) >> 2] = value;
										}
									}
								}
							}
							break;
						case 0x0A: /* Local x2APIC NMI Structure */
							if (length >= 12) {
								unsigned int processorUid = read4(&table[4]);
								if (processorUid == 0xffffffffu || (processorIdValid && processorUid == processorId)) {
									int lint = table[8];
									if (lint == 0 || lint == 1) {
										int flags = read2(&table[2]);
										int pol = flags & 3, tri = (flags >> 2) & 3;
										unsigned int value = 0x00400;
										if (pol == Pol_activeLow) value |= 0x2000;
										if (tri == Tri_level) value |= 0x8000;
										if (x2apic_available) {
											write_msr32(value, 0x835 + lint);
										} else {
											localApic[(0x350 + 0x10 * lint) >> 2] = value;
										}
									}
								}
							}
							break;
					}
				}
				table += length;
			}
			apic_available = 1;
		}
	}

	if (apic_available) {
		if (x2apic_available) {
			interruptMode = INTERRUPT_x2APIC;
		} else {
			interruptMode = INTERRUPT_xAPIC;
		}
		/* enable interrupts for IRQ */
		for (i = 0; i < 16; i++) {
			if (!interruptSourceOverride[i].disabled) {
				volatile unsigned int* addr = ioapics[interruptSourceOverride[i].ioApicId].logicalAddr;
				unsigned int value = 0x10020 + i;
				if (interruptSourceOverride[i].pol == Pol_activeLow) value |= 0x2000;
				if (interruptSourceOverride[i].tri == Tri_level) value |= 0x8000;
				addr[0] = 0x10 + interruptSourceOverride[i].ioApicOffset * 2;
				addr[4] = value;
			}
		}
	} else if (is_8259_available) {
		interruptMode = INTERRUPT_8259;
		/* enable slave -> master interrupt */
		out8_imm8(0xfb, 0x21);
	} else {
		interruptMode = INTERRUPT_UNSUPPORTED;
	}

	if (interruptMode != INTERRUPT_UNSUPPORTED) {
		sti();
		return 1;
	} else {
		return 0;
	}
}

void setInterruptMask(int irq, int mask) {
	unsigned int data;
	if (interruptMode == INTERRUPT_8259) {
		if (0 <= irq && irq < 8) {
			in8_imm8(data, 0x21);
			out8_imm8(mask ? data | (1 << irq) : data & ~(1 << irq), 0x21);
		} else if (8 <= irq && irq < 16) {
			in8_imm8(data, 0xA1);
			out8_imm8(mask ? data | (1 << (irq - 8)) : data & ~(1 << (irq - 8)), 0xA1);
		}
	} else if (interruptMode == INTERRUPT_xAPIC || interruptMode == INTERRUPT_x2APIC) {
		if (0 <= irq && irq < 16 && !interruptSourceOverride[irq].disabled) {
			volatile unsigned int* addr = ioapics[interruptSourceOverride[irq].ioApicId].logicalAddr;
			addr[0] = 0x10 + interruptSourceOverride[irq].ioApicOffset * 2;
			data = addr[4];
			addr[4] = mask ? data | 0x10000 : data & ~0x10000;
		}
	}
}

void registerInterruptHandler(int id, interrupt_handler_type handler) {
	if (0 <= id && id < 256) interruptHandlers[id] = handler;
}

int getIF(void) {
	int eflags;
	__asm__ __volatile__ (
		"pushf\n\t"
		"pop %0\n\t"
	: "=g"(eflags));
	return !!(eflags & 0x200);
}

int isApicTimerAvailable(void) {
	return interruptMode == INTERRUPT_xAPIC || interruptMode == INTERRUPT_x2APIC;
}

void setApicTimerDivider(int divider) {
	unsigned int value;
	if (divider <= 1) value = 0xB;
	else if (divider <= 2) value = 0x0;
	else if (divider <= 4) value = 0x1;
	else if (divider <= 8) value = 0x2;
	else if (divider <= 16) value = 0x3;
	else if (divider <= 32) value = 0x8;
	else if (divider <= 64) value = 0x9;
	else value = 0xA;
	if (interruptMode == INTERRUPT_xAPIC) {
		localApic[0x3E0 >> 2] = value;
	} else if (interruptMode == INTERRUPT_x2APIC) {
		write_msr32(value, 0x83E);
	}
}

void setApicTimerMode(int mode) {
	unsigned int value;
	if (interruptMode == INTERRUPT_xAPIC) {
		value = localApic[0x320 >> 2];
		localApic[0x320 >> 2] = (value & ~(3 << 17)) | ((mode & 3) << 17);
	} else if (interruptMode == INTERRUPT_x2APIC) {
		read_msr32(value, 0x832);
		write_msr32((value & ~(3 << 17)) | ((mode & 3) << 17), 0x832);
	}
}

void setApicTimerInterruptMask(int mask) {
	unsigned int value;
	if (interruptMode == INTERRUPT_xAPIC) {
		value = localApic[0x320 >> 2];
		localApic[0x320 >> 2] = mask ? (value | (1 << 16)) : (value & ~(1 << 16));
	} else if (interruptMode == INTERRUPT_x2APIC) {
		read_msr32(value, 0x832);
		write_msr32(mask ? (value | (1 << 16)) : (value & ~(1 << 16)), 0x832);
	}
}

void setApicTimerInitialCount(unsigned int count) {
	if (interruptMode == INTERRUPT_xAPIC) {
		localApic[0x380 >> 2] = count;
	} else if (interruptMode == INTERRUPT_x2APIC) {
		write_msr32(count, 0x838);
	}
}

unsigned int getApicTimerInitialCount(void) {
	if (interruptMode == INTERRUPT_xAPIC) {
		return localApic[0x380 >> 2];
	} else if (interruptMode == INTERRUPT_x2APIC) {
		unsigned int value;
		read_msr32(value, 0x838);
		return value;
	}
	return 0;
}

unsigned int getApicTimerCurrentCount(void) {
	if (interruptMode == INTERRUPT_xAPIC) {
		return localApic[0x390 >> 2];
	} else if (interruptMode == INTERRUPT_x2APIC) {
		unsigned int value;
		read_msr32(value, 0x839);
		return value;
	}
	return 0;
}

static void commitDelayedFpuSave(void) {
	unsigned int cr0;
	/* if TS = 1 */
	__asm__ __volatile (
		"mov %%cr0, %0\n\t"
	: "=r"(cr0));
	if (cr0 & 8) {
		/* TS = 0 */
		__asm__ __volatile__ (
			"mov %0, %%cr0\n\t"
		: : "r"(cr0 & ~8));
		/* save FPU state if it should actually be saved */
		if (delayedFpuSavePtr != 0) {
			switch (fpuSaveMode) {
				case FPU_NONE:
					/* nothing */
					break;
				case FPU_x87:
					__asm__ __volatile__ (
						"fsave (%0)\n\t"
						"fwait\n\t"
					: : "r"(delayedFpuSavePtr + 1));
					break;
				case FPU_SSE:
					__asm__ __volatile__ (
						"fwait\n\t"
						"fxsave (%0)\n\t"
						"fninit\n\t"
					: : "r"(delayedFpuSavePtr + 1));
					break;
				case FPU_AVX:
					/* initialize XSAVE header */
					{
						int i;
						for (i = 0; i < 16; i++) delayedFpuSavePtr[1 + (512 >> 2) + i] = 0;
					}
					__asm__ __volatile__ (
						"mov $0xffffffff, %%eax\n\t"
						"mov $0xffffffff, %%edx\n\t"
						"fwait\n\t"
						"xsave (%0)\n\t"
						"fninit\n\t"
					: : "r"(delayedFpuSavePtr + 1) : "%eax", "%edx");
					break;
			}
			*delayedFpuSavePtr = FPU_SAVE_YES;
		}
	}
}

void interrupt_handler(struct interrupt_regs* regs, int* fpuSaveStatus) {
	if (regs->interruptId == 7) {
		/* #NM */
		commitDelayedFpuSave();
	}
	/* call interrupt handler */
	if (0 <= regs->interruptId && regs->interruptId < 256 && interruptHandlers[regs->interruptId]) {
		interruptHandlers[regs->interruptId](regs);
	}
	/* send EOI */
	if (interruptMode == INTERRUPT_8259) {
		if (0x20 <= regs->interruptId && regs->interruptId < 0x30) {
			out8_imm8(0x20, 0x20); /* EOI to master */
		}
		if (0x28 <= regs->interruptId && regs->interruptId < 0x30) {
			out8_imm8(0x20, 0xA0); /* EOI to slave */
		}
	} else if (interruptMode == INTERRUPT_xAPIC) {
		if (0x20 <= regs->interruptId && regs->interruptId < 0xff) {
			localApic[0xB0 >> 2] = 0;
		}
	} else if (interruptMode == INTERRUPT_x2APIC) {
		if (0x20 <= regs->interruptId && regs->interruptId < 0xff) {
			write_msr32(0, 0x80B);
		}
	}
	/* restore FPU */
	if (*fpuSaveStatus == FPU_SAVE_MAYBE) {
		/* TS = 0 */
		__asm__ __volatile__ (
			"mov %%cr0, %%eax\n\t"
			"and $0xfffffff7, %%eax\n\t"
			"mov %%eax, %%cr0\n\t"
		: : : "%eax");
	} else if (*fpuSaveStatus == FPU_SAVE_YES) {
		commitDelayedFpuSave();
		/* restore saved status */
		switch (fpuSaveMode) {
			case FPU_NONE:
				/* nothing */
				break;
			case FPU_x87:
				__asm__ __volatile__ (
					"frstor (%0)\n\t"
				: : "r"(fpuSaveStatus + 1));
				break;
			case FPU_SSE:
				__asm__ __volatile__ (
					"fxrstor (%0)\n\t"
				: : "r"(fpuSaveStatus + 1));
				break;
			case FPU_AVX:
				__asm__ __volatile__ (
					"mov $0xffffffff, %%eax\n\t"
					"mov $0xffffffff, %%edx\n\t"
					"xrstor (%0)\n\t"
				: : "r"(fpuSaveStatus + 1) : "%eax", "%edx");
				break;
		}
	}
}
