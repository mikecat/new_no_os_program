#include "memory_utils.h"

int checkSimdAvailability(void) {
	int result = 0;
	unsigned int cpuid_max, cpuid_edx, cpuid_ecx;
	__asm__ __volatile__ (
		"xor %%eax, %%eax\n\t"
		"cpuid\n\t"
		"mov %%eax, %0\n\t"
		"test %%eax, %%eax\n\t"
		"jnz 2f\n\t"
		/* CPUID with eax=1 not supported */
		"xor %%ecx, %%ecx\n\t"
		"xor %%edx, %%edx\n\t"
		"jmp 1f\n\t"
		"2:\n\t"
		"mov $1, %%eax\n\t"
		"cpuid\n\t"
		"1:\n\t"
	: "=m"(cpuid_max), "=d"(cpuid_edx), "=c"(cpuid_ecx) : : "%eax", "%ebx");
	if (cpuid_edx & (1 << 25)) result |= SIMD_AVAILABILITY_SSE;
	if (cpuid_edx & (1 << 26)) result |= SIMD_AVAILABILITY_SSE2;
	if (cpuid_ecx & (1 << 0)) result |= SIMD_AVAILABILITY_SSE3;
	if (cpuid_ecx & (1 << 9)) result |= SIMD_AVAILABILITY_SSSE3;
	if (cpuid_ecx & (1 << 19)) result |= SIMD_AVAILABILITY_SSE41;
	if (cpuid_ecx & (1 << 20)) result |= SIMD_AVAILABILITY_SSE42;
	if (((cpuid_ecx >> 27) & 3) == 3) {
		unsigned int xcr0;
		__asm__ __volatile__ (
			"xor %%ecx, %%ecx\n\t"
			"xgetbv\n\t"
		: "=a"(xcr0) : : "%ecx", "%edx");
		if ((xcr0 & 6) == 6) {
			result |= SIMD_AVAILABILITY_AVX;
			if (cpuid_ecx & (1 << 12)) result |= SIMD_AVAILABILITY_FMA;
			if (cpuid_max >= 7) {
				unsigned int cpuid7_ebx;
				__asm__ __volatile__ (
					"mov $7, %%eax\n\t"
					"xor %%ecx, %%ecx\n\t"
					"cpuid\n\t"
				: "=b"(cpuid7_ebx) : : "%eax", "%ecx", "%edx");
				if (cpuid7_ebx & (1 << 5)) result |= SIMD_AVAILABILITY_AVX2;
			}
		}
	}
	return result;
}

void copyMemory(void* dst, const void* src, unsigned int size) {
	int simd = checkSimdAvailability();
	char* dst_c = dst;
	const char* src_c = src, *src_c_end;
	unsigned int alignMask;
	if (size == 0) return;
	if (size < 0xffffffffu - (unsigned int)src) {
		src_c_end = src + size;
	} else {
		src_c_end = (const char*)0xffffffffu;
	}
	if (simd & SIMD_AVAILABILITY_AVX) alignMask = 32 - 1;
	else if (simd & SIMD_AVAILABILITY_SSE) alignMask = 16 - 1;
	else alignMask = 4 - 1;
	if ((unsigned int)src < (unsigned int)dst) {
		/* copy from high to low */
		dst_c += src_c_end - src_c;
		while (src_c < src_c_end && ((unsigned int)src_c_end & alignMask) != 0) {
			*--dst_c = *--src_c_end;
		}
		if (simd & SIMD_AVAILABILITY_AVX) {
			while (src_c_end - src_c >= 32) {
				dst_c -= 32;
				src_c_end -= 32;
				__asm__ __volatile__ (
					"vmovaps (%0), %%ymm0\n\t"
					"vmovups %%ymm0, (%1)\n\t"
				: : "r"(src_c_end), "r"(dst_c) : "memory");
			}
		} else if (simd & SIMD_AVAILABILITY_SSE) {
			while (src_c_end - src_c >= 16) {
				dst_c -= 16;
				src_c_end -= 16;
				__asm__ __volatile__ (
					"movaps (%0), %%xmm0\n\t"
					"movups %%xmm0, (%1)\n\t"
				: : "r"(src_c_end), "r"(dst_c) : "memory");
			}
		} else {
			while (src_c_end - src_c >= 4) {
				dst_c -= 4;
				src_c_end -= 4;
				*(unsigned int*)dst_c = *(unsigned int*)src_c_end;
			}
		}
		while (src_c < src_c_end) {
			*--dst_c = *--src_c_end;
		}
	} else if (dst != src) {
		/* copy from low to high */
		while (src_c < src_c_end && ((unsigned int)src_c & alignMask) != 0) {
			*dst_c++ = *src_c++;
		}
		if (simd & SIMD_AVAILABILITY_AVX) {
			while (src_c_end - src_c >= 32) {
				__asm__ __volatile__ (
					"vmovaps (%0), %%ymm0\n\t"
					"vmovups %%ymm0, (%1)\n\t"
				: : "r"(src_c), "r"(dst_c) : "memory");
				dst_c += 32;
				src_c += 32;
			}
		} else if (simd & SIMD_AVAILABILITY_SSE) {
			while (src_c_end - src_c >= 16) {
				__asm__ __volatile__ (
					"movaps (%0), %%xmm0\n\t"
					"movups %%xmm0, (%1)\n\t"
				: : "r"(src_c), "r"(dst_c) : "memory");
				dst_c += 16;
				src_c += 16;
			}
		} else {
			while (src_c_end - src_c >= 4) {
				*(unsigned int*)dst_c = *(unsigned int*)src_c;
				dst_c += 4;
				src_c += 4;
			}
		}
		while (src_c < src_c_end){
			*dst_c++ = *src_c++;
		}
	}
}
