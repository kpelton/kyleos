global kernel_bootstrap                           ; making entry point visible to linker
global magic                            ; we will use this in kmain
global mbd                              ; we will use this in kmain
BITS 32 
extern kmain                            ; kmain is defined in kmain.cpp

section .multiboot.data
; Declare constants for the multiboot header.
MBALIGN  equ  1<<0              ; align loaded modules on page boundaries
MEMINFO  equ  1<<1              ; provide memory map
FLAGS    equ  MBALIGN | MEMINFO ; this is the Multiboot 'flag' field
MAGIC    equ  0x1BADB002        ; 'magic number' lets bootloader find the header
CHECKSUM equ -(MAGIC + FLAGS)   ; checksum of above, to prove we are multiboot
 
; Declare a multiboot header that marks the program as a kernel. These are magic
; values that are documented in the multiboot standard. The bootloader will
; search for this signature in the first 8 KiB of the kernel file, aligned at a
; 32-bit boundary. The signature is in its own section so the header can be
; forced to be within the first 8 KiB of the kernel file.
align 4
    dd MAGIC
    dd FLAGS
    dd CHECKSUM
 
section .multiboot.text
align 4
; reserve initial kernel stack space
STACKSIZE equ 0x4000                    ; that's 16k.

GDT64:                           ; Global Descriptor Table (64-bit).
    .Null: equ $ - GDT64         ; The null descriptor.
    dw 0                         ; Limit (low).
    dw 0                         ; Base (low).
    db 0                         ; Base (middle)
    db 0                         ; Access.
    db 0                         ; Granularity.
    db 0                         ; Base (high).
    .Code: equ $ - GDT64         ; The code descriptor.
    dw 0                         ; Limit (low).
    dw 0                         ; Base (low).
    db 0                         ; Base (middle)
    db 10011010b                 ; Access (exec/read).
    db 00100000b                 ; Granularity.
    db 0                         ; Base (high).
    .Data: equ $ - GDT64         ; The data descriptor.
    dw 0                         ; Limit (low).
    dw 0                         ; Base (low).
    db 0                         ; Base (middle)
    db 10010010b                 ; Access (read/write).
    db 00000000b                 ; Granularity.
    db 0                         ; Base (high).
    .Pointer:                    ; The GDT-pointer.
    dw $ - GDT64 - 1             ; Limit.
    dq GDT64  


;Use 2MB pages for initial page table
;Map higher half and and first 2mb 
kernel_bootstrap:
    cli
    ; Move magic value and multiboot header inti edi,rsi which will be the parameters into kmain
    mov esi,eax
    ;jmp $

    mov edi, 0x1000    ; Set the destination index to 0x1000.
    mov cr3, edi       ; Set control register 3 to the destination index.
    xor eax, eax       ; Nullify the A-register.
    mov ecx, 4096      ; Set the C-register to 4096.
    rep stosd          ; Clear the memory.
    mov edi, cr3       ; Set the destination index to control register 3.
    mov DWORD [edi], 0x2003      ; Set the uint32_t at the destination index to 0x2003.
    mov DWORD [edi+8*511], 0x2003      ; Set the uint32_t at the destination index to 0x2003.
    add edi,0x1000
    mov DWORD [edi], 0x3003      ; Set the uint32_t at the destination index to 0x2003.
    mov DWORD [edi+8*510], 0x3003      ; Set the uint32_t at the destination index to 0x2003.
    add edi, 0x1000
    mov eax,0x83
    mov DWORD [edi],eax
    add eax,0x200000
    mov DWORD [edi+8],eax
.do_long_mode:
    mov eax, cr4                 ; Set the A-register to control register 4.
    or eax, 1 << 5               ; Set the PAE-bit, which is the 6th bit (bit 5).
    mov cr4, eax                 ; Set control register 4 to the A-register.
    mov ecx, 0xC0000080          ; Set the C-register to 0xC0000080, which is the EFER MSR.
    rdmsr                        ; Read from the model-specific register.
    or eax, 1 << 8               ; Set the LM-bit which is the 9th bit (bit 8).
    wrmsr                        ; Write to the model-specific register.
    mov eax, cr0                 ; Set the A-register to control register 0.
    or eax, 1 << 31              ; Set the PG-bit, which is the 32nd bit (bit 31).
;    jmp $
    mov cr0, eax                 ; Set control register 0 to the A-register.
;   jmp $
    lgdt [GDT64.Pointer]         ; Load the 64-bit global descriptor table.
    jmp GDT64.Code:Realm64

BITS 64
Realm64:
    mov ax, GDT64.Data            ; Set the A-register to the data descriptor.
    mov ds, ax                    ; Set the data segment to the A-register.
    mov es, ax                    ; Set the extra segment to the A-register.
    mov fs, ax                    ; Set the F-segment to the A-register.
    mov gs, ax                    ; Set the G-segment to the A-register.
    mov ss, ax                    ; Set the stack segment to the A-register.
    mov rax, rsp
    add rax, 0xffffffff80000000   ;Set stack to higher half scheme
    mov rsp,rax 
    ; Move magic value and multiboot header inti edi,rsi which will be the parameters into kmain
    mov edi,ebx
    mov rax, strict qword kmain
    jmp rax

section .bss 
align 0x1000
stack: resb STACKSIZE                   ; reserve 16k stack on a doubleword boundary
stack_end:
magic: resd 1
mbd:   resd 1
