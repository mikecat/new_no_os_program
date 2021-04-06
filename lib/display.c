#include "display.h"
#include "regs.h"
#include "io_macro.h"
#include "pages.h"
#include "text_display.h"
#include "call_uefi.h"

static struct display_info displayInfo;

static int initialized;

struct gop_info {
	int Version, Width, Height, PixelFormat;
	int PixelBitMask[4];
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

struct gop_mode_64 {
	int MaxMode, Mode;
	struct gop_info* info;
	int space1;
	unsigned int infoSize;
	int space2;
	void* vram;
	int something;
	unsigned int vramSize;
};

struct gop_main_64 {
	int (*QueryMode)(struct gop_main* gop, int mode, int* infoSize, struct gop_info** info);
	int space1;
	int (*SetMode)(struct gop_main* gop, int mode);
	int space2;
	void* maybe_function;
	int space3;
	struct gop_mode_64* Mode;
};

static char gop_guid[] = {
	0xde,0xa9,0x42,0x90,0xdc,0x23,0x38,0x4a,
	0x96,0xfb,0x7a,0xde,0xd0,0x80,0x51,0x6a
};

#define ADDR_OFFSET (0x00400000u - 0xC0000000u)

static int initializeDisplayInfo_ident(void* data) {
	struct initial_regs* regs = data;
	if (regs->efer & 0x400) {
		/* Long Mode */
		unsigned int* arg_table = (unsigned int*)regs->iregs.edx;
		unsigned int* services = (unsigned int*)arg_table[24];
		void* LocateProtocol = (void*)services[6 + 2 * 37];
		struct gop_main_64* gops[2] = {0}, *gop;
		struct gop_info* info[2];
		int infoSize[2];
		unsigned int* pde;
		unsigned int logicalVram;
		unsigned int i;
		int res;
		res = call_uefi(regs, LocateProtocol,
			(unsigned int)gop_guid + ADDR_OFFSET, 0, (unsigned int)&gops[0], 0, 0);
		if (res < 0) {
			printfTextDisplay("display: LocateProtocol error 0x%08x\n", (unsigned int)res);
			return 0;
		}
		gop = gops[0];
		res = call_uefi(regs, gop->QueryMode,
			(unsigned int)gop, (unsigned int)(gop->Mode == 0 ? 0 : gop->Mode->Mode),
			(unsigned int)&infoSize[0], (unsigned int)&info[0], 0);
		if (res < 0) {
			printfTextDisplay("display: QueryMode error 0x%08x\n", (unsigned int)res);
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
		if (displayInfo.pixelFormat == 2) {
			for (i = 0; i < 4; i++) displayInfo.pixelBitMask[i] = gop->Mode->info->PixelBitMask[i];
		} else {
			for (i = 0; i < 4; i++) displayInfo.pixelBitMask[i] = 0;
		}
		displayInfo.pixelPerScanLine = gop->Mode->info->PixelPerScanLine;
	} else {
		unsigned int* arg_table = (unsigned int*)((unsigned int*)regs->iregs.esp)[2];
		unsigned int* services = (unsigned int*)arg_table[15];
		void* LocateProtocol = (void*)services[6 + 37];
		struct gop_main* gop = 0;
		struct gop_info* info;
		int infoSize;
		unsigned int* pde;
		unsigned int logicalVram;
		unsigned int i;
		int res;
		res = call_uefi(regs, LocateProtocol,
			(unsigned int)gop_guid + ADDR_OFFSET, 0, (unsigned int)&gop, 0, 0);
		if (res < 0) {
			printfTextDisplay("display: LocateProtocol error 0x%08x\n", (unsigned int)res);
			return 0;
		}
		res = call_uefi(regs, gop->QueryMode,
			(unsigned int)gop, (unsigned int)(gop->Mode == 0 ? 0 : gop->Mode->Mode),
			(unsigned int)&infoSize, (unsigned int)&info, 0);
		if (res < 0) {
			printfTextDisplay("display: QueryMode error 0x%08x\n", (unsigned int)res);
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
		if (displayInfo.pixelFormat == 2) {
			for (i = 0; i < 4; i++) displayInfo.pixelBitMask[i] = gop->Mode->info->PixelBitMask[i];
		} else {
			for (i = 0; i < 4; i++) displayInfo.pixelBitMask[i] = 0;
		}
		displayInfo.pixelPerScanLine = gop->Mode->info->PixelPerScanLine;
	}
	if (displayInfo.pixelFormat < 0 || 2 < displayInfo.pixelFormat) {
		printfTextDisplay("display: unsupported pixel format %d\n", displayInfo.pixelFormat);
		return 0;
	}
	return 1;
}

int initializeDisplayInfo(struct initial_regs* regs) {
	initialized = callWithIdentStack(initializeDisplayInfo_ident, regs);
	return initialized;
}

const struct display_info* getDisplayInfo(void) {
	return initialized ? &displayInfo : 0;
}

unsigned int displayGetColor(int r, int g, int b) {
	if (!initialized) return 0;
	switch (displayInfo.pixelFormat) {
		case 0:
			return ((b & 0xff) << 16) | ((g & 0xff) << 8) | (r & 0xff);
		case 1:
			return ((r & 0xff) << 16) | ((g & 0xff) << 8) | (b & 0xff);
		case 2:
			{
				unsigned int rcount = 0, gcount = 0, bcount = 0;
				unsigned int rvalue, gvalue, bvalue;
				unsigned int result = 0;
				int i;
				for (i = 0; i < 32; i++) {
					if ((displayInfo.pixelBitMask[0] >> i) & 1) rcount++;
					if ((displayInfo.pixelBitMask[1] >> i) & 1) gcount++;
					if ((displayInfo.pixelBitMask[2] >> i) & 1) bcount++;
				}
				if (rcount <= 24) {
					rvalue = (r & 0xff) * ((1 << rcount) - 1) / 255;
				} else {
					rvalue = (r & 0xff) << (rcount - 8);
				}
				if (gcount <= 24) {
					gvalue = (g & 0xff) * ((1 << bcount) - 1) / 255;
				} else {
					gvalue = (g & 0xff) << (gcount - 8);
				}
				if (bcount <= 24) {
					bvalue = (b & 0xff) * ((1 << bcount) - 1) / 255;
				} else {
					bvalue = (b & 0xff) << (bcount - 8);
				}
				for (i = 0; i < 32; i++) {
					if ((displayInfo.pixelBitMask[0] >> i) & 1) {
						result |= rvalue & 1;
						rvalue >>= 1;
					}
					if ((displayInfo.pixelBitMask[1] >> i) & 1) {
						result |= gvalue & 1;
						gvalue >>= 1;
					}
					if ((displayInfo.pixelBitMask[2] >> i) & 1) {
						result |= bvalue & 1;
						bvalue >>= 1;
					}
				}
				return result;
			}
	}
	return 0;
}
