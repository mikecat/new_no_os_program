#include "display.h"
#include "regs.h"
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

typedef int (*call_x64_uefi_type)(struct initial_regs* regs, void* function, void* arg1, int arg2, void* arg3, void* arg4);
extern call_x64_uefi_type call_x64_uefi;

int initializeDisplayInfo_x64(struct initial_regs* regs) {
	unsigned int* arg_table = (unsigned int*)regs->iregs.edx;
	unsigned int* services = (unsigned int*)arg_table[24];
	void* LocateProtocol = (void*)services[6 + 2 * 37];
	call_x64_uefi_type call_x64_uefi_adjusted =
		(call_x64_uefi_type)((unsigned int)&call_x64_uefi + ADDR_OFFSET);
	struct gop_main_64* gops[2] = {0}, *gop;
	struct gop_info* info[2];
	int infoSize[2];
	unsigned int* pde;
	unsigned int logicalVram;
	unsigned int i;
	int res;
	res = call_x64_uefi_adjusted(regs, LocateProtocol,
		(void*)((unsigned int)gop_guid + ADDR_OFFSET), 0, &gops[0], 0);
	if (res < 0) {
		printf_serial_direct("display: LocateProtocol error 0x%08x\n", (unsigned int)res);
		return 0;
	}
	gop = gops[0];
	res = call_x64_uefi_adjusted(regs, gop->QueryMode,
		gop, gop->Mode == 0 ? 0 : gop->Mode->Mode, &infoSize[0], &info[0]);
	if (res < 0) {
		printf_serial_direct("display: QueryMode error 0x%08x\n", (unsigned int)res);
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

int initializeDisplayInfo(struct initial_regs* regs) {
	initialized = 0;
	if (!(regs->efer & 0x400)) { /* Long Mode not enabled at first */
		unsigned int* arg_table = (unsigned int*)((unsigned int*)regs->iregs.esp)[2];
		unsigned int* services = (unsigned int*)arg_table[15];
		int (*LocateProtocol)(void*, void*, struct gop_main**) =
			(int (*)(void*, void*, struct gop_main**))services[6 + 37];
		struct gop_main* gop = 0;
		struct gop_info* info;
		int infoSize;
		unsigned int* pde;
		unsigned int logicalVram;
		unsigned int i;
		int res;
		res = LocateProtocol(gop_guid, 0, &gop);
		if (res < 0) {
			printf_serial_direct("display: LocateProtocol error 0x%08x\n", (unsigned int)res);
			return 0;
		}
		res = gop->QueryMode(gop, gop->Mode == 0 ? 0 : gop->Mode->Mode, &infoSize, &info);
		if (res < 0) {
			printf_serial_direct("display: QueryMode error 0x%08x\n", (unsigned int)res);
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
	} else {
		/* スタックを切り替え、処理を実行する */
		void* newStack = (void*)((unsigned int)getTempBuffer() + ADDR_OFFSET);
		int res;
		__asm__ __volatile__ (
			"push %%ebp\n\t"
			"mov %%esp, %%ebp\n\t"
			"mov %1, %%esp\n\t"
			"and $0xfffffff0, %%esp\n\t"
			"mov %2, (%%esp)\n\t"
			"call _initializeDisplayInfo_x64\n\t"
			"mov %%eax, %0\n\t"
			"leave\n\t" /* mov %%ebp, %%esp; pop %%ebp */
		: "=r"(res) : "r"((char*)newStack + getTempBufferSize() - 4), "r"(regs)
		: "%eax", "%ecx", "%edx");
		if (!res) return 0;
	}
	return 1;
}

const struct display_info* getDisplayInfo(void) {
	return initialized ? &displayInfo : 0;
}
