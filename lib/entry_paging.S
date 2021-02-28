.text
.globl _entry_paging
_entry_paging:
	# save original registers (format of PUSHA)
	sub $48,%esp
	mov %edi, (%esp)
	mov %esi, 4(%esp)
	mov %ebp, 8(%esp)
	mov %ebx, 16(%esp)
	mov %edx, 20(%esp)
	mov %ecx, 24(%esp)
	mov %eax, 28(%esp)
	lea 48(%esp), %eax
	mov %eax, 12(%esp)
	mov %cr0, %eax
	mov %eax, 32(%esp)
	mov %cr3, %eax
	mov %eax, 36(%esp)
	mov %cr4, %eax
	mov %eax, 40(%esp)
	xor %eax, %eax
	mov %eax, 44(%esp) # placeholder for MSR (EFER)

	# disable interrupts
	cli
	# check if cpuid exists
	pushf
	pop %eax # EFLAGS -> EAX
	mov %eax, %edx
	xor $0x00200000, %edx # toggle ID flag
	push %edx
	popf # EDX -> EFLAGS
	pushf
	pop %edx # EFLAGS -> EDX
	xor %edx, %eax
	test $0x00200000, %eax # ID flag in EAX (original) and EDX (toggled) differs?
	jz no_cpuid # if they are same, no CPUID instruction
	# check if MSR exists
	mov $1, %eax
	cpuid
	test $0x20, %edx
	jz no_msr
	# save MSR (EFER) if it exists
	mov $0xc0000080, %ecx
	rdmsr
	mov %eax, 44(%esp)
	# check if information of long mode presence is available
	mov $0x80000000, %eax
	cpuid
	cmp $0x80000001, %eax
	jb no_long_mode_info
	# check if long mode is available
	mov $0x80000001, %eax
	cpuid
	test $0x20000000, %edx
	jz no_long_mode
	# disable paging (for disabling long mode)
	mov %cr0, %eax
	and $0x7fffffff, %eax
	mov %eax, %cr0
	jmp 1f # instruction set changed, flash pipeline?
1:
	# disable long mode
	mov $0xc0000080, %ecx
	rdmsr
	and $0xfffffeff, %eax
	wrmsr
no_cpuid:
no_msr:
no_long_mode_info:
no_long_mode:

	# prepare to pass pointer to original registers as argument
	mov %esp, %eax
	# allocate aligned buffer for page table
	sub $0x2000, %esp
	mov %esp, %edx
	add $0x1000, %edx
	and $0xfffff000, %edx
	mov %eax, (%esp)
	# generate 4MB page table
	xor %ecx, %ecx
gen_loop:
	mov %ecx, %eax
	shl $22, %eax
	cmp $0xc0000000, %eax
	jb no_offset
	# map 0xc0000000 -> 0x00400000
	sub $0xc0000000, %eax
	add $0x00400000, %eax
no_offset:
	or $0x83, %eax
	mov %eax, (%edx, %ecx, 4)
	inc %ecx
	cmp $0x400, %ecx
	jb gen_loop
	# set page table
	mov %edx, %cr3
	# enable page size extension and disable physical address extension
	mov %cr4, %eax
	or $0x00000010, %eax
	and $0xffffffdf, %eax
	mov %eax, %cr4
	# enable paging
	mov %cr0, %eax
	or $0x80000000, %eax
	mov %eax, %cr0
	# jump to C initializer
	mov $_initialize_pages, %eax
	call *%eax
fallback:
	jmp fallback
