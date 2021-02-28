#ifndef DISPLAY_G_GUARD_1ADAC63A_F6B9_425F_A58C_BC944BB1C56B
#define DISPLAY_G_GUARD_1ADAC63A_F6B9_425F_A58C_BC944BB1C56B

#include "regs.h"

struct display_info {
	void* vram;
	unsigned int vramSize;
	int width;
	int height;
	int pixelFormat;
	int pixelPerScanLine;
};

/* ディスプレイの情報を初期化する。成功したら非0、失敗したら0を返す。 */
int initializeDisplayInfo(struct initial_regs* regs);

/* デイスプレイの情報を得る。初期化されていない場合はNULLを返す。 */
const struct display_info* getDisplayInfo(void);

#endif
