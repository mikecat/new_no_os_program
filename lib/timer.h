#ifndef TIMER_H_GUARD_5E399645_0805_4DEE_8D94_87468EC03AE4
#define TIMER_H_GUARD_5E399645_0805_4DEE_8D94_87468EC03AE4

int initializeTimer(void);

int setTimer(unsigned int duration, void (*callback)(void* param), void* callbackParam);

unsigned int getTimerTick(void);

#endif
