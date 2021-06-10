[extern ip_t]
[extern gp]
[extern print_regs]
[extern kbd_irq]       
[extern serial_irq]       
[extern timer_irq]       
[global gdt_flush]
[global load_page_directory]
[global setup_long_mode]
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

[global panic_handler] ; global int handler     
panic_handler:
    call print_regs
    jmp $
    iretq

     
