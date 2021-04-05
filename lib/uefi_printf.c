#include <stdarg.h>
#include "uefi_printf.h"
#include "call_uefi.h"
#include "my_printf.h"

static int initialized = 0;

static unsigned int* ConOut;
static void* OutputString;
static struct initial_regs* regs;

int uefiPrintfInit(struct initial_regs* regs_in) {
	if (regs_in->efer & 0x400) {
		unsigned int* theTable = (unsigned int*)regs_in->iregs.edx;
		ConOut = (unsigned int*)theTable[16];
		OutputString = (void*)ConOut[2];
	} else {
		unsigned int* theTable = (unsigned int*)((unsigned int*)regs_in->iregs.esp)[2];
		ConOut = (unsigned int*)theTable[11];
		OutputString = (void*)ConOut[1];
	}
	regs = regs_in;
	initialized = 1;
	return 0;
}

struct identStackData {
	const char* str;
	int str_len;
};

static int uefiPrintfCallbackForIdentStack(void* data) {
	const char* str = ((struct identStackData*)data)->str;
	int str_len = ((struct identStackData*)data)->str_len;
	char buf[128];
	const int bufLen = sizeof(buf) / sizeof(*buf);
	int count = 0;
	int i;
	for (i = 0; i < str_len; i++) {
		if (str[i] == '\n' && (i == 0 || str[i - 1] != '\r')) {
			buf[count] = '\r';
			buf[count + 1] = '\0';
			count += 2;
			if (count >= bufLen - 2) {
				buf[count] = '\0';
				buf[count + 1] = '\0';
				if (call_uefi(regs, OutputString, (unsigned int)ConOut, (unsigned int)buf, 0, 0, 0) < 0) return 0;
				count = 0;
			}
		}
		buf[count] = str[i];
		buf[count + 1] = '\0';
		count += 2;
		if (count >= bufLen - 2) {
			buf[count] = '\0';
			buf[count + 1] = '\0';
			if (call_uefi(regs, OutputString, (unsigned int)ConOut, (unsigned int)buf, 0, 0, 0) < 0) return 0;
			count = 0;
		}
	}
	if (count > 0) {
		buf[count] = '\0';
		buf[count + 1] = '\0';
		if (call_uefi(regs, OutputString, (unsigned int)ConOut, (unsigned int)buf, 0, 0, 0) < 0) return 0;
		count = 0;
	}
	return 1;
}

static int uefiPrintfCallback(const char* str, int str_len, void* status) {
	struct identStackData isd;
	isd.str = str;
	isd.str_len = str_len;
	(void)status;
	return callWithIdentStack(uefiPrintfCallbackForIdentStack, &isd);
}

int uefiVPrintf(const char* format, va_list args) {
	return my_vprintf_base(uefiPrintfCallback, 0, format, args);
}

int uefiPrintf(const char* format, ...) {
	va_list vl;
	int ret;
	if (!initialized) return -1;
	va_start(vl, format);
	ret = uefiVPrintf(format, vl);
	va_end(vl);
	return ret;
}
