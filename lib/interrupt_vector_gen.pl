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
	pusha
	mov \%esp, \%ecx
	sub \$12, \%esp
	mov \%ds, \%ax
	mov \$0x10, \%dx
	mov \%dx, \%ds
	mov \%ax, 4(\%esp)
	mov \%es, \%ax
	mov \%ax, 6(\%esp)
	mov \%fs, \%ax
	mov \%ax, 8(\%esp)
	mov \%gs, \%ax
	mov \%ax, 10(\%esp)
	mov \%ecx, (\%esp)
	call _interrupt_handler
	mov 6(\%esp), \%ax
	mov \%ax, \%es
	mov 8(\%esp), \%ax
	mov \%ax, \%fs
	mov 10(\%esp), \%ax
	mov \%ax, \%gs
	mov 4(\%esp), \%ax
	mov \%ax, \%ds
	add \$12, \%esp
	popa
	add \$8, \%esp
	iret

.data
.globl _interrupt_handler_list
.balign 4
_interrupt_handler_list:
EOF

for (my $i = 0; $i < 256; $i++) {
	print "\t.long interrupt_vector_$i\n";
}
