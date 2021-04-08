#include "timer.h"
#include "pages.h"
#include "interrupt.h"
#include "acpi.h"
#include "io_macro.h"
#include "text_display.h"

#define TIMER_MAX 512

static int initialized = 0;

struct timerData {
	void (*callback)(void* param);
	void* callbackParam;
	unsigned int durationLeft;
	struct timerData* next;
};

static unsigned int tick = 0;

static struct timerData* timerBuffer;

static struct timerData* timerList;
static struct timerData* timerEmptyList;

static void timerInterruptHandler(struct interrupt_regs* regs);

int initializeTimer(void) {
	unsigned int* fadt = getAcpiTable("FACP", 0);
	unsigned int timerPort = 0, flags = 0;
	int localApicTimerOk = 0;
	unsigned int* cr3;
	int i;
	if (initialized) return 1;

	/* initialize timer buffer */
	get_cr3(cr3);
	timerBuffer = (struct timerData*)allocate_region(cr3, sizeof(*timerBuffer) * TIMER_MAX);
	timerList = 0;
	timerEmptyList = 0;
	for (i = 0; i < TIMER_MAX; i++) {
		timerBuffer[i].next = timerEmptyList;
		timerEmptyList = &timerBuffer[i];
	}
	tick = 0;

	/* initialize timer device */
	if (fadt && fadt[1] >= 116) {
		timerPort = fadt[19]; /* PM_TMR_BLK */
		flags = fadt[28];
		if (fadt[1] >= 220) {
			/* X_PM_TMR_BLK */
			unsigned int* x = &fadt[52];
			if ((x[0] & 1) != 1 || x[2] != 0) {
				printfTextDisplay("timer: unsupported GAS: %08X %08X %08X\n", x[0], x[1], x[2]);
				timerPort = 0;
			} else {
				timerPort = x[1];
			}
		}
		if (timerPort & 0xffff0000u) {
			printfTextDisplay("timer: port too high (0x%08X)\n", timerPort);
			timerPort = 0;
		}
	}

	if (timerPort != 0 && isApicTimerAvailable()) {
		/* use APIC Timer (+ ACPI Power Management Timer for initialization) */
		int is32bitAcpiTimer = flags & (1 << 8);
		int divider;
		setApicTimerMode(APIC_ONE_SHOT);
		setApicTimerInterruptMask(1);
		for (divider = 2; divider <= 128; divider <<= 1) {
			const unsigned int acpiTimerSpeed = 3579545;
			unsigned int acpiTimerInitial, acpiTimerStart;
			unsigned int acpiTimerCurrent, acpiTimerEndStart, acpiTimerEndEnd;
			unsigned int localApicTimerEndTime;
			setApicTimerDivider(divider);
			/* wait for ACPI timer to tick */
			in32(acpiTimerInitial, timerPort);
			do {
				in32(acpiTimerStart, timerPort);
			} while (acpiTimerStart == acpiTimerInitial);
			/* start APIC timer */
			setApicTimerInitialCount(0xffffffffu);
			/* calculate end time for ACPI timer */
			acpiTimerEndStart = acpiTimerStart + acpiTimerSpeed;
			acpiTimerEndEnd = acpiTimerEndStart + acpiTimerSpeed;
			if (!is32bitAcpiTimer) {
				acpiTimerEndStart &= 0x00ffffffu;
				acpiTimerEndEnd &= 0x00ffffffu;
			}
			/* wait 1s */
			if (acpiTimerEndStart < acpiTimerEndEnd) {
				do {
					in32(acpiTimerCurrent, timerPort);
				} while (!(acpiTimerEndStart <= acpiTimerCurrent && acpiTimerCurrent < acpiTimerEndEnd));
			} else {
				do {
					in32(acpiTimerCurrent, timerPort);
				} while (!(acpiTimerCurrent < acpiTimerEndEnd || acpiTimerEndStart <= acpiTimerCurrent));
			}
			localApicTimerEndTime = getApicTimerCurrentCount();
			if (localApicTimerEndTime == 0) {
				/* APIC Timer was too fast, increment divider and retry */
			} else {
				/* initialize APIC Timer */
				setApicTimerMode(APIC_PERIODIC);
				setApicTimerInitialCount((0xffffffffu - localApicTimerEndTime) / 1000);
				registerInterruptHandler(0x30, timerInterruptHandler);
				setApicTimerInterruptMask(0);
				localApicTimerOk = 1;
				break;
			}
		}
	}

	if (!localApicTimerOk) {
		/* use 8253 timer (fallback) */
		int counterSet = 3579545 / (3 * 1000); /* about 1ms interval */
		out8_imm8(0x34, 0x43); /* set timer 0 to pulse generation binary mode */
		out8_imm8(counterSet, 0x40);
		out8_imm8(counterSet >> 8, 0x40);
		registerInterruptHandler(0x20, timerInterruptHandler);
		setInterruptMask(0, 0);
	}

	initialized = 1;
	return 1;
}

static void timerInterruptHandler(struct interrupt_regs* regs) {
	int iflag = getIF();
	(void)regs;
	cli();
	tick++;
	if (timerList) {
		struct timerData* expiredNodes = 0;
		timerList->durationLeft--;
		while (timerList && timerList->durationLeft == 0) {
			struct timerData* thisNode = timerList;
			timerList = timerList->next;
			thisNode->next = expiredNodes;
			expiredNodes = thisNode;
		}
		while (expiredNodes != 0) {
			struct timerData* thisNode = expiredNodes;
			expiredNodes = thisNode->next;
			thisNode->next = timerEmptyList;
			timerEmptyList = thisNode;
			thisNode->callback(thisNode->callbackParam);
		}
	}
	if (iflag) sti();
}

int setTimer(unsigned int duration, void (*callback)(void* param), void* callbackParam) {
	struct timerData* newNode, **newNodePos;
	unsigned int currentDuration = 0;
	int iflag = getIF();
	if (!initialized) return 0;
	cli();
	newNode = timerEmptyList;
	if (newNode == 0) {
		if (iflag) sti();
		return 0;
	}
	timerEmptyList = newNode->next;
	newNode->callback = callback;
	newNode->callbackParam = callbackParam;
	if (duration == 0) {
		if (timerList) timerList->durationLeft--;
		newNode->durationLeft = 1;
		newNode->next = timerList;
		timerList = newNode;
	} else {
		newNodePos = &timerList;
		while (*newNodePos && currentDuration + (*newNodePos)->durationLeft <= duration) {
			currentDuration += (*newNodePos)->durationLeft;
			newNodePos = &(*newNodePos)->next;
		}
		newNode->durationLeft = duration - currentDuration;
		if (*newNodePos) (*newNodePos)->durationLeft -= newNode->durationLeft;
		newNode->next = *newNodePos;
		*newNodePos = newNode;
	}
	if (iflag) sti();
	return 1;
}

unsigned int getTimerTick(void) {
	return tick;
}
