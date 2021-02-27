#include "pusha_regs.h"
#include "serial_direct.h"

struct gop_info {
	int Version, Width, Height, PixelFormat;
	int something[4];
	int PixelPerScanLine;
};

struct gop_mode {
	int MaxMode, Mode;
	struct gop_info* info;
	int infoSize;
	unsigned char* vram;
	int something;
	int vramSize;
};

struct gop_main {
	int (*QueryMode)(struct gop_main* gop, int mode, int* infoSize, struct gop_info** info);
	int (*SetMode)(struct gop_main* gop, int mode);
	void* maybe_function;
	struct gop_mode* Mode;
	unsigned int something[7];
};

static char gop_guid[] = {
	0xde,0xa9,0x42,0x90,0xdc,0x23,0x38,0x4a,
	0x96,0xfb,0x7a,0xde,0xd0,0x80,0x51,0x6a
};

#if 0
void entry(struct pusha_regs* regs) {
	unsigned int* arg_table = (unsigned int*)((unsigned int*)regs->esp)[2];
#else
	void entry(int something, unsigned int* arg_table) {
	(void)something;
#endif
	unsigned int* services = (unsigned int*)arg_table[15];
	int (*LocateProtocol)(void*, void*, struct gop_main**) =
		(int (*)(void*, void*, struct gop_main**))services[6 + 37];
	struct gop_main* gop = 0;
	struct gop_info* info;
	int infoSize;
	int i;
	int res;

	init_serial_direct();
	res = LocateProtocol(gop_guid, 0, &gop);
	if (res < 0) {
		printf_serial_direct("LocateProtocol error 0x%08x\n", (unsigned int)res);
		return;
	}
	printf_serial_direct("gop = %p\n", (void*)gop);

	res = gop->QueryMode(gop, gop->Mode == 0 ? 0 : gop->Mode->Mode, &infoSize, &info);
	if (res < 0) {
		printf_serial_direct("QueryMode error 0x%08x\n", (unsigned int)res);
		return;
	}
	printf_serial_direct("default mode = %d, max mode = %d\n", gop->Mode->Mode, gop->Mode->MaxMode);
	printf_serial_direct("VRAM = 0x%08X, VRAM size = 0x%08X\n",
		(unsigned int)gop->Mode->vram, gop->Mode->vramSize);
	printf_serial_direct("width = %d, height = %d, pixelFormat = %d, pps = %d\n",
		gop->Mode->info->Width, gop->Mode->info->Height,
		gop->Mode->info->PixelFormat, gop->Mode->info->PixelPerScanLine);
	printf_serial_direct("\n"
		"No."
		"    Version"
		"      Width"
		"     Height"
		" PixelFormat"
		" PixelPerScanLine\n");
	for (i = 0; i < gop->Mode->MaxMode; i++) {
		res = gop->QueryMode(gop, i, &infoSize, &info);
		if (res < 0) {
			printf_serial_direct("QueryMode error 0x%08x\n", (unsigned int)res);
			return;
		}
		printf_serial_direct("%3d %10d %10d %10d %11d %16d\n",
			i, info->Version, info->Width, info->Height, info->PixelFormat, info->PixelPerScanLine);
	}
	for (;;) {
		int mode = 0;
		int in;
		printf_serial_direct("select mode: ");
		for (;;) {
			in = getchar_serial_direct();
			if ('0' <= in && in <= '9') mode = mode * 10 + in - '0';
			else if (in == '\b') mode /= 10;
			else if (in == '\n' || in == '\r') break;
		}
		printf_serial_direct("mode %d selected\n", mode);
		res = gop->SetMode(gop, mode);
		if (res < 0) {
			printf_serial_direct("SetMode error 0x%08x\n", (unsigned int)res);
		} else {
			printf_serial_direct("VRAM = 0x%08X, VRAM size = 0x%08X\n",
				(unsigned int)gop->Mode->vram, gop->Mode->vramSize);
			printf_serial_direct("width = %d, height = %d, pixelFormat = %d, pps = %d\n",
				gop->Mode->info->Width, gop->Mode->info->Height,
				gop->Mode->info->PixelFormat, gop->Mode->info->PixelPerScanLine);
		}
	}
}
