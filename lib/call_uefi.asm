bits 32
section .text
global _call_uefi
; ebp - 0x70 : stack?    stack?    stack?    stack?
; ebp - 0x60 : shadow    shadow    shadow    shadow
; ebp - 0x50 : shadow    shadow    shadow    shadow
; ebp - 0x40 : jmp_eip   jmp_cs    5th-arg   5th-arg
; ebp - 0x30 :           gs|fs     es|ss     ds|cs
; ebp - 0x20 : eflags    idt[0]    idt[1]    idt[2]
; ebp - 0x10 : gdt[0]    gdt[1]    gdt[2]    cr3
; ebp - 0x00 : old_ebp   ret_addr  regs      function
; ebp + 0x10 : arg1      arg2      arg3      arg4
; ebp + 0x20 : arg5
_call_uefi:
	; prorogue
	push ebp
	mov ebp, esp
	sub esp, 0x60
	and esp, 0xfffffff0
	; jump to identity-mapped area
	jmp (call_uefi_ident - 0xc0000000 + 0x00400000)
call_uefi_ident:
	; read EFLAGS
	pushf
	pop eax
	mov [ebp - 32], eax
	; disable interrupt
	cli
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
	; backup segment registers
	mov ax, cs
	mov [ebp - 34], ax
	mov ax, ds
	mov [ebp - 36], ax
	mov ax, ss
	mov [ebp - 38], ax
	mov ax, es
	mov [ebp - 40], ax
	mov ax, fs
	mov [ebp - 42], ax
	mov ax, gs
	mov [ebp - 44], ax
	; set CR3 to the original value
	mov ecx, [ebp + 8]
	mov eax, [ecx + 36]
	mov cr3, eax
	; check if Long Mode was enabled
	mov eax, [ecx + 44]
	test eax, 0x00000400
	jz call_uefi_x86
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
	; restore GDTR and IDTR to the original value
	mov ecx, [ebp + 8]
	lgdt [ecx + 48]
	lidt [ecx + 60]
	; enter 64-bit mode
	mov dword [ebp - 64], (long_mode_enabled-0xc0000000+0x00400000)
	mov ax, [ecx + 72]
	mov [ebp - 60], ax
	jmp far dword [ebp - 64]
long_mode_enabled:
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
	mov ecx, [ebp + 16]
	mov edx, [ebp + 20]
	mov r8d, [ebp + 24]
	mov r9d, [ebp + 28]
	mov eax, [ebp + 32]
	mov [esp + 32], rax
	; call the function
	mov eax, [ebp + 12]
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
	mov cx, [ebp - 34]
	movzx rcx, cx
	push rcx
	push (long_mode_enabled_2-0xc0000000+0x00400000)
	db 0x48, 0xcb ; 64-bit retf
bits 32
long_mode_enabled_2:
	; restore segments
	mov cx, [ebp - 36]
	mov ds, cx
	mov cx, [ebp - 38]
	mov ss, cx
	mov cx, [ebp - 40]
	mov es, cx
	mov cx, [ebp - 42]
	mov fs, cx
	mov cx, [ebp - 44]
	mov gs, cx
	; disable paging (exit Long Mode)
	mov ecx, cr0
	and ecx, 0x7fffffff
	mov cr0, ecx
	jmp long_mode_disabled
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
	jmp call_uefi_end

call_uefi_x86:
	; restore GDTR and IDTR to the original value
	lgdt [ecx + 48]
	lidt [ecx + 60]
	; switch CS
	mov dword [ebp - 64], (cs_changed-0xc0000000+0x00400000)
	mov ax, [ecx + 72]
	mov [ebp - 60], ax
	jmp far dword [ebp - 64]
cs_changed:
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
	mov eax, [ebp + 16]
	mov [esp + 0], eax
	mov eax, [ebp + 20]
	mov [esp + 4], eax
	mov eax, [ebp + 24]
	mov [esp + 8], eax
	mov eax, [ebp + 28]
	mov [esp + 12], eax
	mov eax, [ebp + 32]
	mov [esp + 16], eax
	; call the function
	mov eax, [ebp + 12]
	call eax
	cli
	; restore GDT and IDT
	lgdt [ebp - 16]
	lidt [ebp - 28]
	; restore CS
	mov cx, [ebp - 34]
	movzx ecx, cx
	push ecx
	push (cs_restore-0xc0000000+0x00400000)
	retf
cs_restore:
	; restore segments
	mov cx, [ebp - 36]
	mov ds, cx
	mov cx, [ebp - 38]
	mov ss, cx
	mov cx, [ebp - 40]
	mov es, cx
	mov cx, [ebp - 42]
	mov fs, cx
	mov cx, [ebp - 44]
	mov gs, cx

call_uefi_end:
	; restore CR3
	mov ecx, [ebp - 4]
	mov cr3, ecx
	; enable paging
	mov ecx, cr0
	or ecx, 0x80000000
	mov cr0, ecx
	; enable interrupt if it was enabled
	test byte [ebp - 31], 2
	jz call_uefi_no_sti
	sti
call_uefi_no_sti:
	; epilogue
	leave
	ret

extern _tempBuffer
extern _tempBufferSize
global _callWithIdentStack
; ebp - 0x20 : data_arg            seg_work  seg_work
; ebp - 0x10 : gdt       gdt       idt       idt
; ebp - 0x00 : old_ebp   ret_addr  func      data
_callWithIdentStack:
	push ebp
	mov ebp, esp
	sub esp, 0x20
	sgdt [ebp - 0x0e]
	sidt [ebp - 0x06]
	cmp dword [ebp - 0x0c], 0xc0000000
	jb callWithIidentStack_no_gdt_tweak
	mov eax, [ebp - 0x0c]
	add eax, 0x00400000 - 0xc0000000
	mov [ebp - 0x14], eax
	mov ax, [ebp - 0x0e]
	mov [ebp - 0x16], ax
	lgdt [ebp - 0x16]
callWithIidentStack_no_gdt_tweak:
	cmp dword [ebp - 0x04], 0xc0000000
	jb callWithIidentStack_no_idt_tweak
	mov eax, [ebp - 0x04]
	add eax, 0x00400000 - 0xc0000000
	mov [ebp - 0x14], eax
	mov ax, [ebp - 0x06]
	mov [ebp - 0x16], ax
	lidt [ebp - 0x16]
callWithIidentStack_no_idt_tweak:
	mov esp, [_tempBuffer]
	add esp, 0x00400000 - 0xc0000000
	add esp, [_tempBufferSize]
	sub esp, 4
	and esp, 0xfffffff0
	mov eax, [ebp + 12]
	mov [esp + 0], eax
	mov eax, [ebp + 8]
	call eax
	lgdt [ebp - 0x0e]
	lidt [ebp - 0x06]
	leave
	ret
