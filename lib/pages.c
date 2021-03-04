#include "pages.h"
#include "io_macro.h"
#include "regs.h"
#include "panic.h"

unsigned int gdt[] = {
	0, 0,
	0x0000ffffu, 0x00cf9a00u, /* 0x08 ring 0 code segment */
	0x0000ffffu, 0x00cf9200u, /* 0x10 ring 0 data segment */
	0x0000ffffu, 0x00cffa00u, /* 0x08 ring 3 code segment */
	0x0000ffffu, 0x00cff200u, /* 0x10 ring 3 data segment */
	0x0000ffffu, 0x00af9a00u  /* 0x18 ring 0 Long Mode code segment */
};

struct mmap_entry {
	struct mmap_entry *next, *prev;
	unsigned int x3, type, x5, start, x7, end, x9, x10, x11, x12, x13;
};

struct mmap_entry_64 {
	struct mmap_entry_64 *next;
	unsigned int x3;
	struct mmap_entry_64 *prev;
	unsigned int x5, x6, type, start, x9, end, x11, x12, x13, x14, x15;
};

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
		if (physical_pages_refcount[index] > 0) {
			available_physical_pages--;
		} else {
			break;
		}
	}
	if (available_physical_pages == 0) panic("no physical pages available");
	return acquire_ppage(physical_pages[--available_physical_pages]);
}

static unsigned int dist(unsigned int a, unsigned int b) {
	return a < b ? b - a : a - b;
}

static unsigned int search_mmap(unsigned int start, unsigned int end) {
	unsigned int i;
	unsigned int ret = 0;
	if (end < sizeof(struct mmap_entry) + 4) return 0;
	for (i = start & ~3u; i <= end - sizeof(struct mmap_entry) - 4; i += 4) {
		unsigned int* data = (unsigned int*)i;
		if (data[0] == 0x70616d6du && data[6] == 0 && dist(i, data[1]) < 0x2000 && dist(i, data[2]) >= 0x2000) {
			if (ret == 0) {
				ret = i + 4;
			} else {
				return 0xffffffffu;
			}
		}
	}
	return ret;
}

static unsigned int search_mmap_64(unsigned int start, unsigned int end) {
	unsigned int i;
	unsigned int ret = 0;
	if (end < sizeof(struct mmap_entry_64) + 4) return 0;
	for (i = start & ~3u; i <= end - sizeof(struct mmap_entry_64) - 4; i += 4) {
		unsigned int* data = (unsigned int*)i;
		if (data[0] == 0x70616d6du && data[8] == 0 && dist(i, data[2]) < 0x2000 && dist(i, data[4]) >= 0x2000) {
			if (ret == 0) {
				ret = i + 8;
			} else {
				return 0xffffffffu;
			}
		}
	}
	return ret;
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

void initialize_pages(struct initial_regs* regs) {
	unsigned int mmap_addr;
	struct mmap_entry* mmap, *mmap_current, *mmap_end;
	struct mmap_entry_64* mmap_64, *mmap_current_64, *mmap_end_64;

	unsigned char* pe_header = (unsigned char*)0xc0000000;
	unsigned int pe_new_header, pe_opt_header_size;
	unsigned int pe_magic;
	unsigned int pe_start, pe_image_size, pe_stack_size;
	unsigned int stack_start;
	unsigned int current_addr;

	int physical_pages_temp_size;
	unsigned int physical_pages_laddr, physical_pages_refcount_laddr;

	unsigned int* pde_physical;
	unsigned int* next_pde;

	int i;

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

	/* メモリマップを探す */
	mmap_addr = search_mmap(regs->iregs.esp & 0xfe000000u, (regs->iregs.esp & 0xfe000000u) + 0x2000000);
	if (mmap_addr == 0xffffffffu) panic("multiple mmap found");
	if (mmap_addr == 0) {
		mmap_addr = search_mmap_64(regs->iregs.esp & 0xfe000000u, (regs->iregs.esp & 0xfe000000u) + 0x2000000);
		if (mmap_addr == 0) panic("mmap(_64) not found");
		if (mmap_addr == 0xffffffffu) panic("multiple mmap_64 found");
		mmap = 0;
		mmap_end = 0;
		mmap_current = 0;
		mmap_64 = (struct mmap_entry_64*)mmap_addr;
		mmap_end_64 = mmap_64->prev;
		mmap_current_64 = mmap_64;
	} else {
		mmap = (struct mmap_entry*)mmap_addr;
		mmap_end = mmap->prev;
		mmap_current = mmap;
		mmap_64 = 0;
		mmap_end_64 = 0;
		mmap_current_64 = 0;
	}

	/* メモリマップからavailableの場所(プログラムの場所を除く)を一覧にする (作業用の最低限) */
	physical_pages = physical_pages_temp;
	physical_pages_refcount = 0;
	for (current_addr = 0x100000; available_physical_pages < 4096 && current_addr < 0xc0000000u; current_addr += 0x1000) {
		if (pe_start <= current_addr && current_addr < pe_start + pe_image_size) {
			current_addr = pe_start + ((pe_image_size + 0xfff) & ~0xfff);
		}
		if (mmap != 0) {
			while (mmap_current != mmap_end && (current_addr < mmap_current->start || mmap_current->end < current_addr)) {
				mmap_current = mmap_current->next;
			}
			if (mmap_current == mmap_end) break;
			if (mmap_current->type == 7) {
				physical_pages[available_physical_pages++] = current_addr;
			}
		} else {
			while (mmap_current_64 != mmap_end_64 && (current_addr < mmap_current_64->start || mmap_current_64->end < current_addr)) {
				mmap_current_64 = mmap_current_64->next;
			}
			if (mmap_current_64 == mmap_end_64) break;
			if (mmap_current_64->type == 7) {
				physical_pages[available_physical_pages++] = current_addr;
			}
		}
	}
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

	/* メモリマップからavailableの場所(プログラムの場所を除く)を一覧にする (続き) */
	for (; current_addr < 0xc0000000u; current_addr += 0x1000) {
		if (pe_start <= current_addr && current_addr < pe_start + pe_image_size) {
			current_addr = pe_start + ((pe_image_size + 0xfff) & ~0xfff);
		}
		if (mmap != 0) {
			while (mmap_current != mmap_end && (current_addr < mmap_current->start || mmap_current->end < current_addr)) {
				mmap_current = mmap_current->next;
			}
			if (mmap_current == mmap_end) break;
			if (mmap_current->type == 7) {
				physical_pages[available_physical_pages++] = current_addr;
			}
		} else {
			while (mmap_current_64 != mmap_end_64 && (current_addr < mmap_current_64->start || mmap_current_64->end < current_addr)) {
				mmap_current_64 = mmap_current_64->next;
			}
			if (mmap_current_64 == mmap_end_64) break;
			if (mmap_current_64->type == 7) {
				physical_pages[available_physical_pages++] = current_addr;
			}
		}
	}

	/* 全体を4KBページングでマップするためのPDEを用意する */
	next_pde = (unsigned int*)get_ppage();
	for (i = 0; i < 0x300; i++) next_pde[i] = 0;
	for (i = 0x300; i < 0x400; i++) next_pde[i] = pde_physical[i];

	/* メモリマップに載っている0xc0000000以下の場所をマップする */
	if (mmap != 0) {
		for (mmap_current = mmap; mmap_current != mmap_end; mmap_current = mmap_current->next) {
			unsigned int addr;
			for (addr = mmap_current->start & ~0xfff; addr <= mmap_current->end && addr <= 0xc0000000u; addr += 0x1000) {
				if (mmap_current->type == 7) {
					/* available */
					map_pde(next_pde, addr, addr, PAGE_FLAG_WRITE | PAGE_FLAG_PRESENT);
				} else {
					/* available以外 */
					acquire_ppage(addr);
					map_pde(next_pde, addr, addr, PAGE_FLAG_PRESENT);
				}
			}
		}
	} else {
		for (mmap_current_64 = mmap_64; mmap_current_64 != mmap_end_64; mmap_current_64 = mmap_current_64->next) {
			unsigned int addr;
			for (addr = mmap_current_64->start & ~0xfff; addr <= mmap_current_64->end && addr <= 0xc0000000u; addr += 0x1000) {
				if (mmap_current_64->type == 7) {
					/* available */
					map_pde(next_pde, addr, addr, PAGE_FLAG_WRITE | PAGE_FLAG_PRESENT);
				} else {
					/* available以外 */
					acquire_ppage(addr);
					map_pde(next_pde, addr, addr, PAGE_FLAG_PRESENT);
				}
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
