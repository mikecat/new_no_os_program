#ifndef INTERRUPT_G_GUARD_919D12E3_77B3_4434_A527_4AF63654BE10
#define INTERRUPT_G_GUARD_919D12E3_77B3_4434_A527_4AF63654BE10

#include "regs.h"

struct interrupt_regs {
	struct pusha_regs regs;
	int interruptId;
	int errorCode;
	unsigned int eip, cs, eflags, esp, ss;
};

typedef void (*interrupt_handler_type)(struct interrupt_regs* regs);

int initInterrupt(struct initial_regs* regs);

void setInterruptMask(int irq, int mask);

void registerInterruptHandler(int id, interrupt_handler_type handler);

int getIF(void);

#define APIC_ONE_SHOT 0
#define APIC_PERIODIC 1
#define APIC_TSC_DEADLINE 2

int isApicTimerAvailable(void);
void setApicTimerDivider(int divider);
void setApicTimerMode(int mode);
void setApicTimerInterruptMask(int mask);
void setApicTimerInitialCount(unsigned int count);
unsigned int getApicTimerInitialCount(void);
unsigned int getApicTimerCurrentCount(void);

#endif
