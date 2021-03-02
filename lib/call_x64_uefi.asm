bits 32
section .text
global _call_x64_uefi
; int call_x64_uefi(struct initial_regs* regs, void* function,
;                   void* arg1, int arg2, void* arg3, void* arg4)
_call_x64_uefi:
	; prorogue
	push ebp
	mov ebp, esp
	sub esp, 16 + 16 + 16 + 32 ; space for backing up registers + 32 bytes for calling convension
	and esp, 0xfffffff0
	; disable paging
	mov eax, cr0
	and eax, 0x7fffffff
	mov cr0, eax
	; backup CR3
	mov eax, cr3
	mov [ebp - 4], eax
	; backup GDT and IDT
	mov dword [ebp - 12], 0
	mov dword [ebp - 8], 0
	sgdt [ebp - 16]
	mov dword [ebp - 24], 0
	mov dword [ebp - 20], 0
	sidt [ebp - 28]
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
	; restore GDTR and IDTR to the original value
	mov ecx, [ebp + 8]
	lgdt [ecx + 48]
	lidt [ecx + 60]
	; enter 64-bit mode
	mov dword [ebp - 48], (long_mode_enabled_2-0xc0000000+0x00400000)
	mov ax, [ecx + 72]
	mov [ebp - 44], ax
	jmp far dword [ebp - 48]
long_mode_enabled_2:
bits 64
	; set segments to the original value
	mov ax, [ecx + 74]
	mov ds, ax
	mov ax, [ecx + 76]
	mov ss, ax
	mov ax, [ecx + 78]
	mov es, ax
	mov ax, [ecx + 80]
	mov fs, ax
	mov ax, [ecx + 82]
	mov gs, ax
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
	; restore GDT and IDT
	lgdt [ebp - 16]
	lidt [ebp - 28]
	; switch CS (exit 64-bit mode)
	push 0x08
	push (long_mode_enabled_3-0xc0000000+0x00400000)
	db 0x48, 0xcb ; 64-bit retf
long_mode_enabled_3:
	; restore segments
	mov ax, 0x10
	mov ds, ax
	mov ss, ax
	mov es, ax
	mov fs, ax
	mov gs, ax
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
