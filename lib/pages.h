#ifndef PAGES_H_GUARD_8E9EC6B2_1292_4D52_9972_8FE3F9EA91F5
#define PAGES_H_GUARD_8E9EC6B2_1292_4D52_9972_8FE3F9EA91F5

/* 物理ページの参照カウントを増やす */
unsigned int acquire_ppage(unsigned int ppage);
/* 物理ページの参照カウントを減らす */
void release_ppage(unsigned int ppage);
/* 適当な物理ページを確保する */
unsigned int get_ppage(void);

#endif
