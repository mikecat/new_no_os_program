#include "regs.h"
#include "serial_direct.h"
#include "display.h"
#include "pages.h"
#include "io_macro.h"
#include "interrupt.h"

void ud_handler(struct interrupt_regs* regs) {
	printf_serial_direct("#UD thrown at eip=0x%08X\n", regs->eip);
	for (;;) {
		__asm__ __volatile__("hlt\n");
	}
}

void gp_handler(struct interrupt_regs* regs) {
	printf_serial_direct("#GP thrown at eip=0x%08X\n", regs->eip);
	for (;;) {
		__asm__ __volatile__("hlt\n");
	}
}

void entry(struct initial_regs* regs) {
	const struct display_info* display = getDisplayInfo();
	int y, x;
	unsigned int* cr3;
	unsigned char* buffer;
	unsigned int cpuid_ecx, cpuid_edx;
	int enable_sse = 0, enable_avx;
	(void)regs;
	registerInterruptHandler(6, ud_handler);
	registerInterruptHandler(13, gp_handler);

	if (display == 0) {
		printf_serial_direct("display unavailable\n");
		return;
	}
	printf_serial_direct("vram = %p, vramSize = 0x%x\n", display->vram, display->vramSize);
	printf_serial_direct("width = %d, height = %d\n", display->width, display->height);
	printf_serial_direct("pixelFormat = %d, pixelPerScanLine = %d\n", display->pixelFormat, display->pixelPerScanLine);

	get_cr3(cr3);
	buffer = (unsigned char*)allocate_region(cr3, display->vramSize);

	for (y = 0; y < display->height; y++) {
		for (x = 0; x < display->width; x++) {
			unsigned char* pixel = buffer + 4 * (y * display->pixelPerScanLine + x);
			int h = x * 360 / display->width;
			int v = 255 - y * 255 / display->height;
			pixel[3] = 0;
			if (h < 60) {
				pixel[2] = v;
				pixel[1] = v * h / 60;
				pixel[0] = 0;
			} else if (h < 120) {
				pixel[2] = v * (120 - h) / 60;
				pixel[1] = v;
				pixel[0] = 0;
			} else if (h < 180) {
				pixel[2] = 0;
				pixel[1] = v;
				pixel[0] = v * (h - 120) / 60;
			} else if (h < 240) {
				pixel[2] = 0;
				pixel[1] = v * (240 - h) / 60;
				pixel[0] = v;
			} else if (h < 300) {
				pixel[2] = v * (h - 240) / 60;
				pixel[1] = 0;
				pixel[0] = v;
			} else {
				pixel[2] = v;
				pixel[1] = 0;
				pixel[0] = v * (360 - h) / 60;
			}
		}
	}

	__asm__ __volatile__ (
		"mov $1, %%eax\n\t"
		"cpuid\n\t"
	: "=c"(cpuid_ecx), "=d"(cpuid_edx) : : "%eax", "%ebx");
	enable_sse = !!(cpuid_edx & (3u << 24)); /* FXSR and SSE supported? */
	enable_avx = (cpuid_ecx & (5u << 26)) == (5u << 26); /* XSAVE and AVX supported? */

	if (enable_avx) {
		printf_serial_direct("using AVX\n");
		__asm__ __volatile__ (
			/* enable OSXSAVE */
			"mov %%cr4, %%eax\n\t"
			"or $0x40000, %%eax\n\t"
			"mov %%eax, %%cr4\n\t"
			/* enable AVX */
			"xor %%eax, %%eax\n\t"
			"xor %%edx, %%edx\n\t"
			"xor %%ecx, %%ecx\n\t"
			"xgetbv\n\t"
			"or $0x6, %%eax\n\t"
			"xsetbv\n\t"
		: : : "%eax", "%ecx", "%edx", "memory");
	} else if (enable_sse) {
		printf_serial_direct("using SSE\n");
		__asm__ __volatile__ (
			/* enable SSE(OSFXSR) */
			"mov %%cr4, %%eax\n\t"
			"or $0x200, %%eax\n\t"
			"mov %%eax, %%cr4\n\t"
		: : : "%eax", "memory");
	} else {
		printf_serial_direct("AVX/SSE not available\n");
	}

	for (y = 0; y < (int)display->vramSize;) {
		if (enable_avx && y + 32 <= (int)display->vramSize) {
			__asm__ __volatile__ (
				"vmovaps (%0), %%ymm0\n\t"
				"vmovaps %%ymm0, (%1)\n\t"
			: : "r"(buffer + y), "r"((unsigned char*)display->vram + y) : "memory");
			y += 32;
		} else if (enable_sse && y + 16 <= (int)display->vramSize) {
			__asm__ __volatile__ (
				"movaps (%0), %%xmm0\n\t"
				"movaps %%xmm0, (%1)\n\t"
			: : "r"(buffer + y), "r"((unsigned char*)display->vram + y) : "memory");
			y += 16;
		} else if (y + 4 <= (int)display->vramSize) {
			*(unsigned int*)((unsigned char*)display->vram + y) = *(unsigned int*)(buffer + y);
			y += 4;
		} else {
			((unsigned char*)display->vram)[y] = buffer[y];
			y++;
		}
	}
}
