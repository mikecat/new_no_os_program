#ifndef TEXT_DISPLAY_H_GUARD_C31B8B06_FA90_4385_8D61_621CFA76FE23
#define TEXT_DISPLAY_H_GUARD_C31B8B06_FA90_4385_8D61_621CFA76FE23

#include <stdarg.h>

int initializeTextDisplay(void);

void textDisplaySetDrawToScreen(int draw);

int textDisplayGetDisplayLineNum(void);
int textDisplayGetScrollMax(void);
int textDisplayGetScroll(void);
void textDisplaySetScroll(int pos);
void textDisplayMoveScroll(int delta);

int vprintfTextDisplay(const char* format, va_list args);
int printfTextDisplay(const char* format, ...);

#endif
