#ifndef PAGES_H_GUARD_8E9EC6B2_1292_4D52_9972_8FE3F9EA91F5
#define PAGES_H_GUARD_8E9EC6B2_1292_4D52_9972_8FE3F9EA91F5

#define PAGE_FLAG_PRESENT 0x001
#define PAGE_FLAG_WRITE 0x002
#define PAGE_FLAG_USER 0x004
#define PAGE_FLAG_WRITE_THROUGH 0x008
#define PAGE_FLAG_CACHE_DISABLE 0x010
#define PAGE_FLAG_ACCESS 0x020
#define PAGE_FLAG_DIRTY 0x040
#define PAGE_FLAG_GLOBAL 0x100

/* 物理ページの参照カウントを増やす */
unsigned int acquire_ppage(unsigned int ppage);
/* 物理ページの参照カウントを減らす */
void release_ppage(unsigned int ppage);
/* 適当な物理ページを確保する */
unsigned int get_ppage(void);

/* PDEの論理アドレスladdrを、物理アドレスpaddrにマップする */
void map_pde(unsigned int* pde, unsigned int laddr, unsigned int paddr, int flags);

/* 仮想メモリに領域を確保する (物理ページを割り当てる) */
unsigned int allocate_region(unsigned int* pde, unsigned int size);

/* 仮想メモリに空間を確保する (物理ページは割り当てない) */
unsigned int allocate_region_without_allocation(unsigned int size);

#endif
