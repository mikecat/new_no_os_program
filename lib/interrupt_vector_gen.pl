#!/usr/bin/perl

use strict;
use warnings;

print ".text\n";
for (my $i = 0; $i < 256; $i++) {
	print "interrupt_vector_$i:\n";
	if ($i <= 7 || $i == 9 || $i == 15 || $i == 16 || (18 <= $i && $i <= 29) || 31 <= $i) {
		print "\tpushl \$0\n";
	}
	print "\tpushl \$$i\n";
	print "\tjmp interrupt_handler_call\n";
}

print <<EOF;

interrupt_handler_call:
	# save registers (allocate 8 for segment and 8 for arguments)
	pusha
	mov \%esp, \%ecx
	mov \%esp, \%ebp
	sub \$16, \%esp
	mov \%ds, \%ax
	mov \$0x10, \%dx
	mov \%dx, \%ds
	mov \%ax, (\%ebp)
	mov \%es, \%ax
	mov \%ax, -2(\%ebp)
	mov \%fs, \%ax
	mov \%ax, -4(\%ebp)
	mov \%gs, \%ax
	mov \%ax, -6(\%ebp)
	cmpl \$0, (fpuSaveMode)
	jz 1f
	# jump if TS = 1
	mov \%cr0, \%eax
	test \$8, \%eax
	jnz 1f
	# allocate for FPU save
	sub (fpuSaveSize), \%esp
	lea 8(\%esp), \%edx
	# FPU save alignment (allocate at lease 4 byte more -> FPU save status)
	mov (fpuSaveAlignmentSize), \%eax
	sub \%eax, \%esp
	dec \%eax
	not \%eax
	and \%eax, \%edx
	# allocate for FPU save status
	sub \$4, \%edx
	# FPU save status = MAYBE
	movl \$1, (\%edx)
	# register delayed FPU save
	mov \%edx, (delayedFpuSavePtr)
	# turn on TS
	mov \%cr0, \%eax
	or \$8, \%eax
	mov \%eax, \%cr0
	jmp 2f
1:
	# FPU save status = NO
	pushl \$0
	mov \%esp, \%edx
2:
	# 16-byte stack alignment
	and \$0xfffffff0, \%esp
	# put arguments
	mov \%edx, 4(\%esp)
	mov \%ecx, (\%esp)
	# call C interrupt handler
	call interrupt_handler

	# restore registers
	mov -2(\%ebp), \%ax
	mov \%ax, \%es
	mov -4(\%ebp), \%ax
	mov \%ax, \%fs
	mov -6(\%ebp), \%ax
	mov \%ax, \%gs
	mov (\%ebp), \%ax
	mov \%ax, \%ds
	mov \%ebp, \%esp
	popa
	add \$8, \%esp
	iret

.data
.globl interrupt_handler_list
.balign 4
interrupt_handler_list:
EOF

for (my $i = 0; $i < 256; $i++) {
	print "\t.long interrupt_vector_$i\n";
}
