#include "regs.h"
#include "serial_direct.h"
#include "display.h"

void entry(struct initial_regs* regs) {
	const struct display_info* display = getDisplayInfo();
	int y, x;
	(void)regs;
	if (display == 0) {
		printf_serial_direct("display unavailable\n");
		return;
	}
	printf_serial_direct("vram = %p, vramSize = 0x%x\n", display->vram, display->vramSize);
	printf_serial_direct("width = %d, height = %d\n", display->width, display->height);
	printf_serial_direct("pixelFormat = %d, pixelPerScanLine = %d\n", display->pixelFormat, display->pixelPerScanLine);

	for (y = 0; y < display->height; y++) {
		for (x = 0; x < display->width; x++) {
			unsigned char* pixel = (unsigned char*)display->vram + 4 * (y * display->pixelPerScanLine + x);
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
}
