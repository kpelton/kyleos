CC	= gcc 
CFLAGS	= -m64 -Wall -Wextra -nostdlib -fno-builtin -nostartfiles -nodefaultlibs -ffreestanding -mcmodel=large -mno-red-zone -mno-mmx -mno-sse -mno-sse2 -no-pie
LD	= ld  -m elf_x86_64 -no-pie
 
OBJFILES = asm.o kernel.o loader.o vga.o tables.o asm_calls.o irq.o  paging.o output.o uart.o ata.o
#OBJFILES =  loader.o 
 
all: kernel.img
 
.s.o:
	nasm -f elf64  -o $@ $<
 
.c.o:
	$(CC) $(CFLAGS) -o $@ -c $<
 
kernel.bin: $(OBJFILES)
	$(LD)  -T linker.ld -o $@ $^ 
 
kernel.img: kernel.bin
	objcopy   -I elf64-x86-64 -O elf32-i386   kernel.bin kernel32.bin
	
clean:
	$(RM) $(OBJFILES) kernel.bin kernel.img kernel32.bin
 
test: kernel32.bin
	qemu-system-x86_64 -kernel kernel32.bin -d int,cpu_reset -hda test-hd.img 2>log
