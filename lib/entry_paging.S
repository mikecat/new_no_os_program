.text
.globl _entry_paging
_entry_paging:
	# save original registers
	pusha
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
	and $0xffffffbf, %eax
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
