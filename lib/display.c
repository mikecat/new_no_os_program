#include "display.h"
#include "pusha_regs.h"
#include "io_macro.h"
#include "pages.h"
#include "serial_direct.h"

static struct display_info displayInfo;

static int initialized;

struct gop_info {
	int Version, Width, Height, PixelFormat;
	int something[4];
	int PixelPerScanLine;
};

struct gop_mode {
	int MaxMode, Mode;
	struct gop_info* info;
	unsigned int infoSize;
	void* vram;
	int something;
	unsigned int vramSize;
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

int initializeDisplayInfo(struct pusha_regs* regs) {
	unsigned int* arg_table = (unsigned int*)((unsigned int*)regs->esp)[2];
	unsigned int* services = (unsigned int*)arg_table[15];
	int (*LocateProtocol)(void*, void*, struct gop_main**) =
		(int (*)(void*, void*, struct gop_main**))services[6 + 37];
	struct gop_main* gop = 0;
	struct gop_info* info;
	int infoSize;
	int res;
	unsigned int* pde;
	unsigned int logicalVram;
	unsigned int i;
	initialized = 0;
	res = LocateProtocol(gop_guid, 0, &gop);
	if (res < 0) {
		printf_serial_direct("display: LocateProtocol error 0x%08x\n", (unsigned int)res);
		return 0;
	}
	res = gop->QueryMode(gop, gop->Mode == 0 ? 0 : gop->Mode->Mode, &infoSize, &info);
	if (res < 0) {
		printf_serial_direct("display:QueryMode error 0x%08x\n", (unsigned int)res);
		return 0;
	}

	get_cr3(pde);
	logicalVram = allocate_region_without_allocation(gop->Mode->vramSize);
	for (i = 0; i < gop->Mode->vramSize; i += 0x1000) {
		map_pde(pde, logicalVram + i, (unsigned int)gop->Mode->vram + i,
			PAGE_FLAG_PRESENT | PAGE_FLAG_WRITE | PAGE_FLAG_CACHE_DISABLE);
	}

	displayInfo.vram = (void*)logicalVram;
	displayInfo.vramSize = gop->Mode->vramSize;
	displayInfo.width = gop->Mode->info->Width;
	displayInfo.height = gop->Mode->info->Height;
	displayInfo.pixelFormat = gop->Mode->info->PixelFormat;
	displayInfo.pixelPerScanLine = gop->Mode->info->PixelPerScanLine;
	initialized = 1;
	return 1;
}

const struct display_info* getDisplayInfo(void) {
	return initialized ? &displayInfo : 0;
}
