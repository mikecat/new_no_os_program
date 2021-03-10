#ifndef UEFI_PRINTF_H_GUARD_D144F515_7242_45AC_B7F8_3D619D8FC5B0
#define UEFI_PRINTF_H_GUARD_D144F515_7242_45AC_B7F8_3D619D8FC5B0

#include "regs.h"

int uefiPrintfInit(struct initial_regs* regs_in);
int uefiPrintf(const char* format, ...);

#endif
