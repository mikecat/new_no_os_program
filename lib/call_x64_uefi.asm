bits 32
section .text
global _call_x64_uefi
; int call_x64_uefi(struct initial_regs* regs, void* function,
;                   void* arg1, int arg2, void* arg3, void* arg4)
_call_x64_uefi:
	; prorogue
	push ebp
	mov ebp, esp
	sub esp, 48
	and esp, 0xfffffff0
	; disable paging
	mov eax, cr0
	and eax, 0x7fffffff
	mov cr0, eax
	; backup CR3
	mov eax, cr3
	mov [ebp - 4], eax
	; set CR3 to the original value
	mov eax, [ebp + 8]
	mov eax, [eax + 36]
	mov cr3, eax
	; enable PAE
	mov eax, cr4
	or eax, 0x20
	mov cr4, eax
	; enable Long Mode
	mov ecx, 0xc0000080
	rdmsr
	or eax, 0x100
	wrmsr
	; enable paging (enter Long Mode : compatible mode)
	mov eax, cr0
	or eax, 0x80000000
	mov cr0, eax
	jmp long_mode_enabled
long_mode_enabled:
	; enter 64-bit mode
	mov dword [ebp - 16], (long_mode_enabled_2-0xc0000000+0x00400000)
	mov ax, cs
	mov [ebp - 12], ax
	jmp far dword [ebp - 16]
long_mode_enabled_2:
bits 64
	; set up arguments
	mov ecx, [rbp + 16]
	mov edx, [rbp + 20]
	mov r8d, [rbp + 24]
	mov r9d, [rbp + 28]
	; call the function
	mov eax, [rbp + 12]
	call rax
	cli
	; adjust return value
	test rax, rax
	jns no_8
	or eax, 0x80000000
no_8:
	; disable paging (exit Long Mode)
	mov rcx, cr0
	and ecx, 0x7fffffff
	mov cr0, rcx
	jmp long_mode_disabled
bits 32
long_mode_disabled:
	; disable PAE
	mov ecx, cr4
	and ecx, 0xffffffdf
	mov cr4, ecx
	; disable Long Mode
	push eax
	mov ecx, 0xc0000080
	rdmsr
	and eax, 0xfffffeff
	wrmsr
	pop eax
	; restore CR3
	mov ecx, [ebp - 4]
	mov cr3, ecx
	; enable paging
	mov ecx, cr0
	or ecx, 0x80000000
	mov cr0, ecx
	; epilogue
	leave
	ret
