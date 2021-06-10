CC	= gcc
AS = nasm
ASFLAGS = -f elf64
KERNEL_ROOT=$(shell pwd)
CFLAGS	= -m64 -Wall -Wextra -nostdlib -fno-builtin -nostartfiles -nodefaultlibs -ffreestanding -mcmodel=large -mno-red-zone -mno-mmx -mno-sse -mno-sse2 -no-pie -I $(KERNEL_ROOT) -g
LD	= ld  -m elf_x86_64 -no-pie
export CFLAGS 
export CC
export AS
export ASFLAGS
SUBDIRS = $(shell ls -d */)
OBJ_FILES = $(shell find . -type f -name '*.o')
all: kernel.img
LIBS:
	for dir in $(SUBDIRS) ; do \
		make -C  $$dir ; \
	done

clean: 
	for dir in $(SUBDIRS) ; do \
		make -C  $$dir clean ; \
	done
	rm -rfv *.o
	rm -rfv kernel.bin
	rm -rfv kernel32.bin

kernel.bin: LIBS
	$(LD) -T linker.ld -o kernel.bin $(OBJ_FILES)

kernel.img: kernel.bin
	objcopy   -I elf64-x86-64 -O elf32-i386   kernel.bin kernel32.bin

test: kernel32.bin
	qemu-system-x86_64 -m 8G -kernel kernel32.bin -d int,cpu_reset -hda test-hd.img -serial stdio 2>log
kvm-test: kernel32.bin
	qemu-system-x86_64 -m 8G -kernel kernel32.bin -d int,cpu_reset -hda test-hd.img -serial stdio -enable-kvm 2>log


test-c: kernel32.bin
	qemu-system-x86_64 -m 8G -kernel kernel32.bin -d int,cpu_reset -hda test-hd.img -display none -serial stdio 2>log
debug: kernel32.bin
	qemu-system-x86_64 -m 8G -kernel kernel32.bin -hda test-hd.img -display none -serial stdio -s -S
