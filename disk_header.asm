bits 16
org 0x7C00

jmp program
nop

db "xxxxxxxx" ; OEName
dw 512 ; BytsPerSec
db 1 ; SecPerClus
dw 1 ; RsvdSecCnt
db 2 ; NumFATs
dw 14 * 512 / 32 ; RootEntCnt
dw 0x0B40 ; TotSec16
db 0xF0 ; Media
dw 9 ; FatSz16
dw 18 ; SecPerTrk
dw 2 ; NumHeads
dd 0 ; HiddSec
dd 0 ; TotSec32

db 0 ; DrvNum
db 0 ; Reserved
db 0x29 ; BootSig
db 0xDE,0xAD,0xBE,0xEF ; VolID
db "NO NAME    " ; VolLab
db "FAT12   " ; FilSysType

program:
	mov ax, 0x1301
	mov bx, 0x000F
	mov cx, str_end - str_start
	xor dx, dx
	mov es, dx
	mov bp, str_start
	int 0x10
stop:
	cli
	hlt
	jmp stop

str_start:
	db "Booted from BIOS, stop.", 0x0D, 0x0A
str_end:
