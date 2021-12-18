[extern ip_t]
[extern gp]
[extern print_regs]
[extern kbd_irq]
[extern serial_irq]
[extern kprintf]
[extern ksleepm]
[extern timer_irq]
[extern rtc_irq]
[global gdt_flush]
[global load_page_directory]
[global setup_long_mode]
HelloString db 'hello',0xa, 0


gdt_flush:
 mov rax, strict qword gp
 lgdt [rax]
  mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    jmp flush2

flush2:
    ret

[global tss_flush]
tss_flush:
    mov ax, 5*8 | 3
    ltr ax
    ret

test_user_function2:
    add eax,123
    ;;int 0x80
    ;;sti
    ret
[global test_user_function]
test_user_function:
    mov rax,123
    int 0x80
    ;sti
    call test_user_function2
    jmp test_user_function
    ret

[global test_user_function5]
test_user_function5:
    mov rax,100
    int 0x80
    ;sti
    call test_user_function2
    jmp test_user_function
    ret



[global usermode_int] ;
usermode_int:
    mov rdi,rax
    call ksleepm
    mov rdi,HelloString
    ;call kprintf
    ;jmp panic_handler
    mov ax, (4 * 8)
	mov ds, ax
	mov es, ax 
	mov fs, ax 
    iretq

[global jump_usermode]

;rdi address to jump to
;rsi user stack
jump_usermode:
	mov ax, (4 * 8) | 3 ; ring 3 data with bottom 2 bits set for ring 3
	mov ds, ax
	mov es, ax 
	mov fs, ax 
	mov gs, ax ; SS is handled by iret
	; set up the stack frame iret expects
	push (4 * 8) | 3 ; data selector
	push rsi ; current esp
    pushf
	push (3 * 8) | 3 ; code selector (ring 3 code with bottom 2 bits set for ring 3)
	push rdi ; instruction address to return to
    sti
	iretq

setup_long_mode:
  ret 
[global idt_flush] ; make 'gdt_flush' accessible from C code
idt_flush:
    mov rax, strict qword ip_t
    lidt [rax]
	ret

[global std_handler] ; global int handler
std_handler:
    iretq
[global kbd_handler] ; global int handler
kbd_handler:
    push rax
    push rbx
    push rcx
    push rdx
    push rsi
    push rdi
    push rbp
    push r8
    push r9
    push r10
    push r11
    push r12
    push r13
    push r14
    push r15
    pushfq
    mov ax, (2 * 8)
	mov ds, ax
	mov es, ax 
	mov fs, ax 
    call kbd_irq
    popfq
    pop r15
    pop r14
    pop r13
    pop r12
    pop r11
    pop r10
    pop r9
    pop r8
    pop rbp
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rbx
    pop rax
    iretq

[global timer_handler] ; global int handler
timer_handler:
    
    push rax
    push rbx
    push rcx
    push rdx
    push rsi
    push rdi
    push rbp
    push r8
    push r9
    push r10
    push r11
    push r12
    push r13
    push r14
    push r15
    pushfq
    mov ax, (2 * 8)
	mov ds, ax
	mov es, ax 
	mov fs, ax 
	mov gs, ax ; SS is handled by iret
    call timer_irq
    popfq
    pop r15
    pop r14
    pop r13
    pop r12
    pop r11
    pop r10
    pop r9
    pop r8
    pop rbp
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rbx
    pop rax
    iretq

[global serial_handler] ; global int handler
serial_handler:
    push rax
    push rbx
    push rcx
    push rdx
    push rsi
    push rdi
    push rbp
    push r8
    push r9
    push r10
    push r11
    push r12
    push r13
    push r14
    push r15
    pushfq
    mov ax, (2 * 8)
	mov ds, ax
	mov es, ax 
	mov fs, ax 
    call serial_irq
    popfq
    pop r15
    pop r14
    pop r13
    pop r12
    pop r11
    pop r10
    pop r9
    pop r8
    pop rbp
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rbx
    pop rax
    iretq

[global rtc_handler] ; global int handler
rtc_handler:
    push rax
    push rbx
    push rcx
    push rdx
    push rsi
    push rdi
    push rbp
    push r8
    push r9
    push r10
    push r11
    push r12
    push r13
    push r14
    push r15
    pushfq
    mov ax, (2 * 8)
    mov ds, ax
    mov es, ax
    mov fs, ax
    call rtc_irq
    popfq
    pop r15
    pop r14
    pop r13
    pop r12
    pop r11
    pop r10
    pop r9
    pop r8
    pop rbp
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rbx
    pop rax
    iretq


[global switch_to] ; global int handler
switch_to:
    mov rsp,rdi
    mov rax,rsi
    sti
    jmp rax
    jmp $

[global resume_p] ; global int handler
resume_p:
    mov rsp,rdi
    mov rbp,rsi
    sub rsp ,8
    sti
    ret

[global panic_handler] ; global int handler
panic_handler:
    cli
    mov rdi,rax; Exception code
    mov rsi,[rsp+8] ; RIP is pushed on stack
    call print_regs
    jmp $
    iretq
[global panic_handler_1] ;
panic_handler_1:
    mov rax,1
    jmp panic_handler
[global panic_handler_2] ;
panic_handler_2:
    mov rax,2
    jmp panic_handler

[global panic_handler_3] ;
panic_handler_3:
    mov rax,3
    jmp panic_handler

[global panic_handler_4] ;
panic_handler_4:
    mov rax,4
    jmp panic_handler

[global panic_handler_5] ;
panic_handler_5:
    mov rax,5
    jmp panic_handler

[global panic_handler_6] ;
panic_handler_6:
    mov rax,6
    jmp panic_handler

[global panic_handler_7] ;
panic_handler_7:
    mov rax,7
    jmp panic_handler

[global panic_handler_8] ;
panic_handler_8:
    mov rax,8
    jmp panic_handler

[global panic_handler_9] ;
panic_handler_9:
    mov rax,9
    jmp panic_handler

[global panic_handler_10] ;
panic_handler_10:
    mov rax,10
    jmp panic_handler

[global panic_handler_11] ;
panic_handler_11:
    mov rax,11
    jmp panic_handler

[global panic_handler_12] ;
panic_handler_12:
    mov rax,12
    jmp panic_handler

[global panic_handler_13] ;
panic_handler_13:
    mov rax,13
    jmp panic_handler

[global panic_handler_14] ;
panic_handler_14:
    mov rax,14
    jmp panic_handler
[global panic_handler_15] ;
panic_handler_15:
    mov rax,15
    jmp panic_handler

[global panic_handler_16] ;
panic_handler_16:
    mov rax,16
    jmp panic_handler
[global panic_handler_17] ;
panic_handler_17:
    mov rax,17
    jmp panic_handler

[global panic_handler_18] ;
panic_handler_18:
    mov rax,18
    jmp panic_handler
[global panic_handler_19] ;
panic_handler_19:
    mov rax,19
    jmp panic_handler
[global panic_handler_20] ;
panic_handler_20:
    mov rax,20
    jmp panic_handler
[global panic_handler_21] ;
panic_handler_21:
    mov rax,21
    jmp panic_handler
[global panic_handler_22] ;
panic_handler_22:
    mov rax,22
    jmp panic_handler
[global panic_handler_23] ;
panic_handler_23:
    mov rax,23
    jmp panic_handler
[global panic_handler_24] ;
panic_handler_24:
    mov rax,24
    jmp panic_handler
[global panic_handler_25] ;
panic_handler_25:
    mov rax,25
    jmp panic_handler
[global panic_handler_26] ;
panic_handler_26:
    mov rax,26
    jmp panic_handler
[global panic_handler_27] ;
panic_handler_27:
    mov rax,27
    jmp panic_handler
[global panic_handler_28] ;
panic_handler_28:
    mov rax,28
    jmp panic_handler
[global panic_handler_29] ;
panic_handler_29:
    mov rax,29
    jmp panic_handler
[global panic_handler_30] ;
panic_handler_30:
    mov rax,30
    jmp panic_handler
[global panic_handler_31] ;
panic_handler_31:
    mov rax,31
    jmp panic_handler
[global panic_handler_32] ;
panic_handler_32:
    mov rax,32
    jmp panic_handler
