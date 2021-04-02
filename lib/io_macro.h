#ifndef IO_H_GUARD_55CD09BF_7165_4595_9F46_E68648124D54
#define IO_H_GUARD_55CD09BF_7165_4595_9F46_E68648124D54

#define cli() __asm__ __volatile__ ("cli\n\t")
#define sti() __asm__ __volatile__ ("sti\n\t")
#define hlt() __asm__ __volatile__ ("hlt\n\t")

#define in8_imm8(data, port) __asm__ __volatile__ \
	("xor %%eax, %%eax\n\tin %1, %%al\n\t" : "=a"(data) : "N"(port))

#define in16_imm8(data, port) __asm__ __volatile__ \
	("xor %%eax, %%eax\n\tin %1, %%ax\n\t" : "=a"(data) : "N"(port))

#define in8(data, port) __asm__ __volatile__ \
	("xor %%eax, %%eax\n\tin %%dx, %%al\n\t" : "=a"(data) : "d"(port))

#define in16(data, port) __asm__ __volatile__ \
	("xor %%eax, %%eax\n\tin %%dx, %%ax\n\t" : "=a"(data) : "d"(port))

#define out8_imm8(data, port) __asm__ __volatile__ \
	("out %%al, %1\n\t" : : "a"(data), "N"(port))

#define out16_imm8(data, port) __asm__ __volatile__ \
	("out %%ax, %1\n\t" : : "a"(data), "N"(port))

#define out8(data, port) __asm__ __volatile__ \
	("out %%al, %%dx\n\t" : : "a"(data), "d"(port))

#define out16(data, port) __asm__ __volatile__ \
	("out %%ax, %%dx\n\t" : : "a"(data), "d"(port))

#define get_cr3(data) __asm__ __volatile__ \
	("mov %%cr3, %0\n\t" : "=r"(data))

#define set_cr3(data) __asm__ __volatile__ \
	("mov %0, %%cr3\n\t" : : "r"(data))

#define read_msr(low, high, addr) __asm__ __volatile__ ( \
	"rdmsr\n\t" : "=a"(low), "=d"(high) : "c"(addr))

#define write_msr(low, high, addr) __asm__ __volatile__ ( \
	"wrmsr\n\t" : : "a"(low), "d"(high), "c"(addr))

#define read_msr32(data, addr) __asm__ __volatile__ ( \
	"rdmsr\n\t" : "=a"(data) : "c"(addr) : "%edx")

#define write_msr32(data, addr) __asm__ __volatile__ ( \
	"wrmsr\n\t" : : "a"(data), "c"(addr))

#endif
