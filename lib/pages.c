#include "pages.h"
#include "io_macro.h"
#include "regs.h"
#include "panic.h"
#include "call_uefi.h"
#include "serial_direct.h"

unsigned int gdt[] = {
	0, 0,
	0x0000ffffu, 0x00cf9a00u, /* 0x08 ring 0 code segment */
	0x0000ffffu, 0x00cf9200u, /* 0x10 ring 0 data segment */
	0x0000ffffu, 0x00cffa00u, /* 0x08 ring 3 code segment */
	0x0000ffffu, 0x00cff200u  /* 0x10 ring 3 data segment */
};

unsigned int gdt_size = sizeof(gdt);

struct mmap_element {
	unsigned int type;
	unsigned int space;
	unsigned int start[2];
	unsigned int virtualAddress[2];
	unsigned int numPages[2];
	unsigned int attribute[2];
};

static char memory_map[4096 * 4];

static int available_physical_pages;
static unsigned int* physical_pages;
static unsigned int* physical_pages_refcount;
static unsigned int physical_pages_temp[4096];

const void* tempBuffer = physical_pages_temp;
const unsigned int tempBufferSize = sizeof(physical_pages_temp);

static unsigned int heap_address;

unsigned int acquire_ppage(unsigned int ppage) {
	unsigned int index;
	if (physical_pages_refcount == 0) return ppage;
	if (ppage & 0xfff) panic("unaligned acquire_ppage()");
	index = ppage >> 12;
	if (physical_pages_refcount[index] == 0xffffffffu) panic("too many physical page references");
	physical_pages_refcount[index]++;
	return ppage;
}

void release_ppage(unsigned int ppage) {
	unsigned int index;
	if (physical_pages_refcount == 0) return;
	if (ppage & 0xfff) panic("unaligned release_ppage()");
	index = ppage >> 12;
	if (physical_pages_refcount[index] == 0) panic("released unreferenced physical page");
	physical_pages_refcount[index]--;
	if (physical_pages_refcount[index] == 0) {
		physical_pages[available_physical_pages++] = ppage;
	}
}

unsigned int get_ppage(void) {
	while (available_physical_pages > 0) {
		unsigned int index = (unsigned int)physical_pages[available_physical_pages - 1] >> 12;
		if (physical_pages_refcount && physical_pages_refcount[index] > 0) {
			available_physical_pages--;
		} else {
			break;
		}
	}
	if (available_physical_pages == 0) panic("no physical pages available");
	return acquire_ppage(physical_pages[--available_physical_pages]);
}

static unsigned int read2(const unsigned char* pos) {
	return pos[0] | (pos[1] << 8);
}

static unsigned int read4(const unsigned char* pos) {
	return pos[0] | (pos[1] << 8) | (pos[2] << 16) | (pos[3] << 24);
}

void map_pde(unsigned int* pde, unsigned int laddr, unsigned int paddr, int flags) {
	unsigned int pde_index = (laddr >> 22) & 0x3ff;
	unsigned int pte_index = (laddr >> 12) & 0x3ff;
	unsigned int* pte;
	int i;
	if (!(pde[pde_index] & 1)) {
		pte = (unsigned int*)get_ppage();
		for (i = 0; i < 1024; i++) pte[i] = 0;
		pde[pde_index] = ((unsigned int)pte & ~0xfff) | (PAGE_FLAG_WRITE | PAGE_FLAG_PRESENT);
	} else {
		pte = (unsigned int*)(pde[pde_index] & ~0xfff);
	}
	pte[pte_index] = (paddr & ~0xfff) | (flags & 0xfff);
}

unsigned int allocate_region(unsigned int* pde, unsigned int size) {
	unsigned int ptr;
	unsigned int i;
	size = (size + 0xfff) & ~0xfff;
	ptr = heap_address - size;
	for (i = 0; i < size; i += 0x1000) {
		unsigned int* buffer = (unsigned int*)get_ppage();
		int j;
		for (j = 0; j < 1024; j++) buffer[j] = 0;
		map_pde(pde, ptr + i, (unsigned int)buffer, PAGE_FLAG_WRITE | PAGE_FLAG_PRESENT);
	}
	heap_address = ptr - 0x1000;
	return ptr;
}

unsigned int allocate_region_without_allocation(unsigned int size) {
	unsigned int ptr;
	size = (size + 0xfff) & ~0xfff;
	ptr = heap_address - size;
	heap_address = ptr - 0x1000;
	return ptr;
}

/*
アドレスaddrがelementの範囲より前 → 負の数を返す
アドレスaddrがelementの範囲内 → 0を返す
アドレスaddrがelementの範囲より後 → 正の数を返す
*/
static int compareElement(unsigned int addr, const struct mmap_element* element) {
	if (element->start[1] || addr < element->start[0]) {
		/* 指定の要素の範囲より前 */
		return -1;
	}
	if (element->numPages[1] || ((addr - element->start[0]) >> 12) < element->numPages[0]) {
		/* 指定の要素の開始地点からの距離(ページ数)が、指定の要素のページ数未満 */
		return 0;
	}
	return 1;
}

void initialize_pages(struct initial_regs* regs) {
	void* GetMemoryMap;
	int mmap_ret;
	unsigned int mmap_sizes[2] = {sizeof(memory_map), 0}, mmap_size;
	unsigned int mmap_key[2];
	unsigned int mmap_element_sizes[2] = {0, 0}, mmap_element_size;
	unsigned int mmap_version[2];

	unsigned char* pe_header = (unsigned char*)0xc0000000;
	unsigned int pe_new_header, pe_opt_header_size;
	unsigned int pe_magic;
	unsigned int pe_start, pe_image_size, pe_stack_size;
	unsigned int stack_start = 0;

	int physical_pages_temp_size;
	unsigned int physical_pages_laddr, physical_pages_refcount_laddr;

	unsigned int* pde_physical = 0;
	unsigned int* next_pde;

	unsigned int current_addr;
	int i, j;

	/* シリアルポートを初期化する (panicで使うので) */
	init_serial_direct();

	/* PEヘッダの情報を読み取る */
	if (read2(pe_header) != 0x5a4d) panic("PE header Magic mismatch");
	pe_new_header = read4(pe_header + 0x3c);
	if (pe_new_header >= 0x10000000u) panic("PE new header too far");
	if (read4(pe_header + pe_new_header) != 0x4550) panic("PE header Signature mismatch");
	pe_opt_header_size = read2(pe_header + pe_new_header + 0x14);
	if (pe_opt_header_size < 0x78) panic("PE optional header too small");
	pe_magic = read2(pe_header + pe_new_header + 0x18);
	if (pe_magic == 0x010B) {
		pe_start = read4(pe_header + pe_new_header + 0x34);
	} else if (pe_magic == 0x020B) {
		pe_start = read4(pe_header + pe_new_header + 0x30);
	} else {
		pe_start = 0;
		panic("PE magic unsupported");
	}
	pe_image_size = read4(pe_header + pe_new_header + 0x50);
	pe_stack_size = read4(pe_header + pe_new_header + 0x60);
	if (pe_start & 0xfff) panic("PE image not aligned");
	if (pe_start >= 0xc0000000u) panic("PE image too high");
	if (pe_image_size >= 0x10000000u) panic("PE image too large");
	if (pe_stack_size >= 0x10000000u) panic("PE stack too large");

	/* メモリマップを取得する */
	if (regs->efer & 0x400) {
		/* Long Mode */
		unsigned int* table = (unsigned int*)regs->iregs.edx;
		unsigned int* services = (unsigned int*)table[24];
		GetMemoryMap = (void*)services[6 + 2 * 4];
	} else {
		unsigned int* table = (unsigned int*)((unsigned int*)regs->iregs.esp)[2];
		unsigned int* services = (unsigned int*)table[15];
		GetMemoryMap = (void*)services[6 + 4];
	}
	mmap_ret = call_uefi(regs, GetMemoryMap, (unsigned int)mmap_sizes,
		(unsigned int)memory_map - 0xc0000000u + 0x00400000u,
		(unsigned int)mmap_key, (unsigned int)mmap_element_sizes, (unsigned int)mmap_version);
	if (mmap_ret < 0) panic("GetMemoryMap failed");
	if (mmap_sizes[1]) panic("memory map too large");
	if (mmap_element_sizes[1]) panic("memory map element too large");
	mmap_size = mmap_sizes[0];
	mmap_element_size = mmap_element_sizes[0];
	if (mmap_element_size < sizeof(struct mmap_element)) panic("memory map element too small");
	/* メモリマップの内容をチェックする */
	for (i = 0; i + mmap_element_size <= mmap_size; i += mmap_element_size) {
		struct mmap_element* element = (struct mmap_element*)(memory_map + i);
		unsigned int endAddr;
		if (element->start[1]) {
			/* pages that exceeds 4GB won't be used */
			continue;
		}
		if (element->start[0] & 0xfff) {
			/* this won't happen */
			panic("memory map not aligned");
		}
		if (element->numPages[1] || element->numPages[0] >= 0x100000u) {
			/* the size is >= 4GB */
			endAddr = 0xffffffffu;
		} else if (element->numPages[0] > 0) {
			endAddr = (element->start[0] >> 12) + element->numPages[0];
			if (endAddr >= 0x100000u) {
				/* the end is >= 4GB */
				endAddr = 0xffffffffu;
			} else {
				endAddr = (endAddr << 12) - 1;
			}
		} else {
			/* zero-page entry */
			continue;
		}
		for (j = 0; j < i; j += mmap_element_size) {
			struct mmap_element* element2 = (struct mmap_element*)(memory_map + j);
			if (compareElement(element->start[0], element2) > 0 ||
			compareElement(endAddr, element2) < 0) {
				/* no collision, OK */
			} else {
				/* collision */
				panic("memory map overwrapped");
			}
		}
	}

	/* GDTを高位に移動する (UEFI関数呼び出しによるメモリマップ取得の後で行う) */
	__asm__ __volatile__ (
		"sub $8, %%esp\n\t"
		"mov %0, 4(%%esp)\n\t"
		"mov %1, %%eax\n\t"
		"dec %%eax\n\t"
		"mov %%ax, 2(%%esp)\n\t"
		"lgdt 2(%%esp)\n\t"
		"add $8, %%esp\n\t"
	: : "r"(gdt), "r"(gdt_size) : "%eax");

	/* メモリマップからavailableの場所(プログラムの場所を除く)を一覧にする */
	physical_pages = physical_pages_temp;
	physical_pages_refcount = 0;
	for (i = 0; i + mmap_element_size <= mmap_size; i += mmap_element_size) {
		struct mmap_element* element = (struct mmap_element*)(memory_map + i);
		if (element->type == 7) {
			for (current_addr = element->start[0], j = 0;
			current_addr < 0xc0000000u && (element->numPages[1] || (unsigned int)j < element->numPages[0]);
			j++, current_addr += 0x1000) {
				physical_pages[available_physical_pages++] = current_addr;
				if (physical_pages_refcount == 0 && available_physical_pages >= (int)(sizeof(physical_pages_temp) / sizeof(*physical_pages_temp))) {
					/* 十分なメモリが確保できたので、切り替えを行う */
					physical_pages_temp_size = available_physical_pages;

					stack_start = (0xfffff000u - pe_stack_size) & ~0xfff;
					heap_address = stack_start - 0x1000;

					/* PDEを確保する */
					pde_physical = (unsigned int*)get_ppage();
					for (i = 0; i < 0x300; i++) pde_physical[i] = (i << 22) | 0x83;
					for (i = 0x300; i < 0x400; i++) pde_physical[i] = 0;

					/* プログラムをマップする */
					for (current_addr = pe_start; current_addr < pe_start + pe_image_size; current_addr += 0x1000) {
						map_pde(pde_physical, current_addr - pe_start + 0xc0000000u, current_addr, PAGE_FLAG_WRITE | PAGE_FLAG_PRESENT);
					}

					/* availableな場所一覧と参照カウント用のメモリを確保する */
					physical_pages_laddr = allocate_region(pde_physical, 0x400000);
					physical_pages_refcount_laddr = allocate_region(pde_physical, 0x400000);

					/* PDEを切り替える */
					set_cr3(pde_physical);

					/* 確保した一覧の場所を設定する */
					physical_pages = (unsigned int*)physical_pages_laddr;
					physical_pages_refcount = (unsigned int*)physical_pages_refcount_laddr;
					for (i = 0; i < available_physical_pages; i++) {
						physical_pages[i] = physical_pages_temp[i];
					}
					/* 使用した分の物理ページの参照を入れる */
					for (i = available_physical_pages; i < physical_pages_temp_size; i++) {
						acquire_ppage(physical_pages_temp[i]);
					}
					/* プログラムの参照を入れる */
					for (current_addr = pe_start; current_addr < pe_start + pe_image_size; current_addr += 0x1000) {
						acquire_ppage(current_addr);
					}
				}
			}
		}
	}

	if (physical_pages_refcount == 0) {
		/* 切り替え処理が走らないほどメモリが少ない (Availableが16MiB無かった) */
		panic("too few memory");
	}

	/* 全体を4KBページングでマップするためのPDEを用意する */
	next_pde = (unsigned int*)get_ppage();
	for (i = 0; i < 0x300; i++) next_pde[i] = 0;
	for (i = 0x300; i < 0x400; i++) next_pde[i] = pde_physical[i];

	/* メモリマップに載っている0xc0000000未満の場所をマップする */
	for (i = 0; i + mmap_element_size <= mmap_size; i += mmap_element_size) {
		struct mmap_element* element = (struct mmap_element*)(memory_map + i);
		for (current_addr = element->start[0], j = 0;
		current_addr < 0xc0000000u && (element->numPages[1] || (unsigned int)j < element->numPages[0]);
		j++, current_addr += 0x1000) {
			if (element->type == 7) {
				/* available */
				map_pde(next_pde, current_addr, current_addr, PAGE_FLAG_WRITE | PAGE_FLAG_PRESENT);
			} else {
				/* available以外 */
				acquire_ppage(current_addr);
				map_pde(next_pde, current_addr, current_addr, PAGE_FLAG_PRESENT);
			}
		}
	}

	/* プログラムをマップする (書き込みを許可) */
	for (current_addr = pe_start; current_addr < pe_start + pe_image_size; current_addr += 0x1000) {
		map_pde(next_pde, current_addr, current_addr, PAGE_FLAG_WRITE | PAGE_FLAG_PRESENT);
	}

	/* スタックを確保する */
	for (current_addr = stack_start; current_addr < 0xfffff000u; current_addr += 0x1000) {
		unsigned int addr = get_ppage();
		map_pde(next_pde, current_addr, addr, PAGE_FLAG_WRITE | PAGE_FLAG_PRESENT);
	}

	/* 旧になるPDEを開放する */
	release_ppage((unsigned int)pde_physical);

	/* スタックを移動し、関数を実行する */
	__asm__ __volatile__ (
		"mov %0, %%cr3\n\t"
		"mov $0xffffee00, %%esp\n\t"
		"mov %1, (%%esp)\n\t"
		"push %1\n\t"
		"call _library_initialize\n\t"
		"pop %%eax\n\t"
		"call _entry\n\t"
		"1:\n\t"
		"jmp 1b\n\t"
	: : "r"(next_pde), "r"(regs));
}
