#include "text_display.h"
#include "pages.h"
#include "display.h"
#include "font.h"
#include "io_macro.h"
#include "memory_utils.h"
#include "my_printf.h"
#include "serial_direct.h"
#include "uefi_printf.h"

/* 初期化前に出力された文字を初期化時に描画するために保存する用のバッファサイズ */
#define BEFORE_INITIALIZE_BUFFER_MAX 4096
/* 保持する最大行数 */
#define SCREEN_LINE_MAX 10000
/* スクロールバーの大きさ */
#define SCROLL_BAR_REGION_WIDTH 16
#define SCROLL_BAR_WIDTH 8
/* 画面端の余白の大きさ */
#define MARGIN_WIDTH 0
#define MARGIN_HEIGHT 0
/* 文字を描画する大きさ */
#define CHAR_DRAW_WIDTH 8
#define LINE_HEIGHT 16

static int initialized = 0;
static int panicMode = 0;

static int beforeInitializeBufferSize = 0;
static char beforeInitializeBuffer[BEFORE_INITIALIZE_BUFFER_MAX];

static int screenPixelWidth, screenPixelHeight, screenPixelStride;
static void* vram;
static unsigned int vramSize;

static int characterWidth, characterHeight;
static unsigned char* characterData;
static unsigned int* displayBuffer;

static int drawToScreen;

static int bufferStart, bufferEnd, outputColumnPos;
static int bufferDisplayStart;
static int prevIsCr;

static void putOneChar(int c);
static void renderTextDisplay(void);

int initializeTextDisplay(void) {
	unsigned int* cr3;
	const struct display_info* di;
	int i;
	if (initialized) return 1;
	/* get display information */
	di = getDisplayInfo();
	if (di == 0) return 0;
	screenPixelWidth = di->width;
	screenPixelHeight = di->height;
	screenPixelStride = di->pixelPerScanLine;
	vram = di->vram;
	vramSize = di->vramSize;
	/* initialize buffer information */
	characterWidth = (screenPixelWidth - MARGIN_WIDTH * 2 - SCROLL_BAR_REGION_WIDTH) / CHAR_DRAW_WIDTH;
	characterHeight = (screenPixelHeight - MARGIN_HEIGHT * 2) / LINE_HEIGHT;
	bufferStart = 0;
	bufferEnd = 0;
	outputColumnPos = 0;
	bufferDisplayStart = 0;
	drawToScreen = 1;
	/* allocate buffer */
	get_cr3(cr3);
	characterData = (unsigned char*)allocate_region(cr3, characterWidth * SCREEN_LINE_MAX);
	displayBuffer = (unsigned int*)allocate_region(cr3, vramSize);

	/* mark as initialized */
	initialized = 1;
	/* flush text printed before initializing */
	for (i = 0; i < beforeInitializeBufferSize; i++) {
		putOneChar((unsigned char)beforeInitializeBuffer[i]);
	}
	renderTextDisplay();

	return 1;
}

void textDisplayPanic(void) {
	if (initialized) {
		drawToScreen = 1;
		textDisplaySetScroll(textDisplayGetScrollMax());
	} else {
		if (beforeInitializeBufferSize > 0) {
			char bak = beforeInitializeBuffer[beforeInitializeBufferSize - 1];
			beforeInitializeBuffer[beforeInitializeBufferSize - 1] = 0;
			uefiPrintf("%s%c", beforeInitializeBuffer, bak);
		}
	}
	panicMode = 1;
}

void textDisplaySetDrawToScreen(int draw) {
	if (initialized) {
		int requestRedraw = !drawToScreen && draw;
		drawToScreen = draw;
		if (requestRedraw) renderTextDisplay();
	}
}

int textDisplayGetDisplayLineNum(void) {
	return characterHeight;
}

int textDisplayGetScrollMax(void) {
	int allLines = bufferEnd - bufferStart + 1;
	if (allLines <= 0) allLines += SCREEN_LINE_MAX;
	if (allLines < characterHeight) {
		return 0;
	} else {
		return allLines - characterHeight;
	}
}

int textDisplayGetScroll(void) {
	int currentLineStart = bufferDisplayStart - bufferStart;
	if (currentLineStart < 0) currentLineStart += SCREEN_LINE_MAX;
	return currentLineStart;
}

void textDisplaySetScroll(int pos) {
	if (initialized) {
		int scrollTo = pos, scrollMax = textDisplayGetScrollMax();
		if (scrollTo < 0) scrollTo = 0;
		if (scrollTo > scrollMax) scrollTo = scrollMax;
		bufferDisplayStart = bufferStart + scrollTo;
		if (bufferDisplayStart >= SCREEN_LINE_MAX) bufferDisplayStart -= SCREEN_LINE_MAX;
		if (drawToScreen) renderTextDisplay();
	}
}

void textDisplayMoveScroll(int delta) {
	textDisplaySetScroll(textDisplayGetScroll() + delta);
}

static void putOneChar(int c) {
	int newline = 0;
	if (c == '\r' || (c == '\n' && !prevIsCr)) {
		/* go to new line */
		newline = 1;
	} else if (prevIsCr && c == '\n') {
		/* do nothing: ignore LF in CRLF */
	} else {
		characterData[characterWidth * bufferEnd + outputColumnPos++] = c;
		if (outputColumnPos >= characterWidth) newline = 1;
	}
	if (newline) {
		int i;
		if (characterHeight < SCREEN_LINE_MAX &&
		(bufferDisplayStart == bufferEnd - characterHeight + 1 ||
		bufferDisplayStart == bufferEnd - characterHeight + 1 + SCREEN_LINE_MAX)) {
			bufferDisplayStart++;
			if (bufferDisplayStart >= SCREEN_LINE_MAX) bufferDisplayStart = 0;
		}
		bufferEnd++;
		outputColumnPos = 0;
		if (bufferEnd >= SCREEN_LINE_MAX) bufferEnd = 0;
		if (bufferStart == bufferEnd) {
			int updateDisplayStart = bufferDisplayStart == bufferStart;
			bufferStart++;
			if (bufferStart >= SCREEN_LINE_MAX) bufferStart = 0;
			if (updateDisplayStart) bufferDisplayStart = bufferStart;
		}
		for (i = 0; i < characterWidth; i++) {
			characterData[characterWidth * bufferEnd + i] = 0;
		}
	}
	prevIsCr = c == '\r';
}

static void renderTextDisplay(void) {
	int yCount, y, x;
	int allLines, currentLineStart, currentLineEnd;
	int scrollBarStart, scrollBarEnd, scrollBarX;
	/* draw text */
	for (y = bufferDisplayStart, yCount = 0; yCount < characterHeight; yCount++) {
		for (x = 0; x < characterWidth; x++) {
			int oy = yCount * LINE_HEIGHT + (LINE_HEIGHT - FONT_HEIGHT) / 2;
			int ox = x * CHAR_DRAW_WIDTH + (CHAR_DRAW_WIDTH - FONT_WIDTH) / 2;
			int c = characterData[characterWidth * y + x];
			int py, px;
			for (py = 0; py < FONT_HEIGHT; py++) {
				for (px = 0; px < FONT_WIDTH; px++) {
					displayBuffer[(oy + py) * screenPixelStride + (ox + px)] =
						(font_data[c][py] >> (FONT_WIDTH - 1 - px)) & 1 ? 0xffffff : 0x000000;
				}
			}
		}
		/* proceed to next line */
		if (y == bufferEnd) break;
		y++;
		if (y >= SCREEN_LINE_MAX) y = 0;
	}
	/* draw scroll bar */
	allLines = bufferEnd - bufferStart + 1;
	if (allLines <= 0) allLines += SCREEN_LINE_MAX;
	currentLineStart = bufferDisplayStart - bufferStart;
	if (currentLineStart < 0) currentLineStart += SCREEN_LINE_MAX;
	currentLineEnd = currentLineStart + characterHeight;
	if (currentLineEnd > allLines) currentLineEnd = allLines;
	scrollBarStart = screenPixelHeight * currentLineStart / allLines;
	scrollBarEnd = screenPixelHeight * currentLineEnd / allLines;
	scrollBarX = screenPixelWidth - MARGIN_WIDTH - SCROLL_BAR_REGION_WIDTH +
		(SCROLL_BAR_REGION_WIDTH - SCROLL_BAR_WIDTH) / 2;
	for (y = 0; y < screenPixelHeight; y++) {
		for (x = 0; x < SCROLL_BAR_WIDTH; x++) {
			displayBuffer[y * screenPixelStride + (scrollBarX + x)] =
				scrollBarStart <= y && y < scrollBarEnd ? 0xC0C0C0 : 0x404040;
		}
	}
	/* send to screen */
	copyMemory(vram, displayBuffer, vramSize);
}

static int printfTextDisplayCallback(const char* str, int str_len, void* status) {
	int i;
	(void)status;
	for (i = 0; i < str_len; i++) {
		int c = str[i] == 0 ? ' ' : (unsigned char)str[i];
		if (initialized) {
			putOneChar(c);
		} else {
			if (beforeInitializeBufferSize >= BEFORE_INITIALIZE_BUFFER_MAX) return 0;
			beforeInitializeBuffer[beforeInitializeBufferSize++] = c;
		}
		putchar_serial_direct(str[i]);
	}
	return 1;
}

int vprintfTextDisplay(const char* format, va_list args) {
	return my_vprintf_base(printfTextDisplayCallback, 0, format, args);
}

int printfTextDisplay(const char* format, ...) {
	va_list vl;
	int ret;
	va_start(vl, format);
	if (!initialized && panicMode) {
		ret = uefiVPrintf(format, vl);
	} else {
		ret = vprintfTextDisplay(format, vl);
	}
	va_end(vl);
	if (initialized && drawToScreen) renderTextDisplay();
	return ret;
}
