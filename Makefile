CC	?= gcc
AS = nasm
ASFLAGS = -f elf64
KERNEL_ROOT=$(shell pwd)
CFLAGS	= -m64 -O0 -Wall -Wextra -nostdlib -fno-builtin -nostartfiles -nodefaultlibs -ffreestanding -mcmodel=large -mno-red-zone -mno-mmx -mno-sse -mno-sse2  -I $(KERNEL_ROOT)  -g 
LD	= ld  -m elf_x86_64 
export CFLAGS 
export CC
export AS
export ASFLAGS
SUBDIRS = $(shell ls -d */)
OBJ_FILES = $(shell find . -type f -name '*.o')

all: kernel.img user

install: user
	$(MAKE) -C user install

utils: utils/llist.o
	$(MAKE) -C utils

asm: asm/asm.o asm/asm_calls.o
	$(MAKE) -C asm

block: block/ata.o 
	$(MAKE) -C block

fs: fs/fat.o fs/vfs.o fs/ramfs.o
	$(MAKE) -C block

init: init/kernel.o init/loader.o init/tables.o init/dshell.o init/syscall.o
	$(MAKE) -C init

irq: irq/irq.o
	$(MAKE) -C irq

mm: mm/mm.o mm/paging.o mm/pmem.o mm/vmm.o
	$(MAKE) -C mm

sched: sched/sched.o sched/exec.o sched/ps.o
	$(MAKE) -C sched

output: output/output.o output/vga.o output/uart.o output/keyboard.o output/input.o
	$(MAKE) -C output

timer: timer/pit.o timer/timer.o timer/rtc.o
	$(MAKE) -C timer

locks: locks/spinlock.o locks/mutex.o
	$(MAKE) -C locks

user:kernel.img
	$(MAKE) -C user
    
clean: 
	for dir in $(SUBDIRS) ; do \
		make -C  $$dir clean ; \
	done
	rm -rfv *.o
	rm -rfv kernel.bin
	rm -rfv kernel32.bin

kernel.bin: asm block init irq mm output timer sched fs locks utils
	$(LD) -T linker.ld -o kernel.bin $(OBJ_FILES) 

kernel.img: kernel.bin
	objcopy   -I elf64-x86-64 -O elf32-i386   kernel.bin kernel32.bin

test: kernel32.bin
	qemu-system-x86_64 -m 64M -kernel kernel32.bin  -hda test-hd.img -serial stdio -rtc base=localtime

test-nox: kernel32.bin
	qemu-system-x86_64 -m 4G -kernel kernel32.bin  -hda test-hd.img -display none -serial stdio -rtc base=localtime
test-log: kernel32.bin
	qemu-system-x86_64 -m 8G -kernel kernel32.bin -d int,cpu_reset -hda test-hd.img -serial stdio 2>log
kvm-test: kernel32.bin
	qemu-system-x86_64 -m 8G -kernel kernel32.bin -d int,cpu_reset -hda test-hd.img -serial stdio -enable-kvm 2>log

test-c: kernel32.bin
	qemu-system-x86_64 -m 8G -kernel kernel32.bin -d int,cpu_reset -hda test-hd.img -display none -serial stdio 2>log
debug: kernel32.bin
	qemu-system-x86_64 -m 8G -kernel kernel32.bin -hda test-hd.img -serial stdio -s -S
debug-nox: kernel32.bin
	qemu-system-x86_64 -m 8G -display none -kernel kernel32.bin -hda test-hd.img -serial stdio -s -S

gdb: kernel.bin
	gdb -ex "target remote localhost:1234" kernel.bin
iso: kernel.bin
	cp kernel.bin iso
	grub-mkrescue -o bootable.iso iso
iso-test: bootable.iso
	qemu-system-x86_64 -m 60M -cdrom bootable.iso  -serial stdio
test-suite:kernel.bin
	bash test.sh 2>/dev/null
