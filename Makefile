CC	?= gcc
.DEFAULT_GOAL := all
OP ?= -O0
AS = nasm
ASFLAGS = -f elf64
KERNEL_ROOT=$(shell pwd)
IMAGE ?= $(KERNEL_ROOT)/build/image/test-hd.img
IMAGE_MOUNT ?= $(KERNEL_ROOT)/build/image/mnt
USERLAND_STAGE ?= $(KERNEL_ROOT)/build/userland
CORE_SRC ?= $(KERNEL_ROOT)/../../kyleos-userspace
PROGS_SRC ?= $(KERNEL_ROOT)/../../newlib-progs
NEWLIB_BUILD ?= $(KERNEL_ROOT)/../../newlib-build
SYSROOT ?= /tmp/z
DOOM_BINARY ?= $(KERNEL_ROOT)/build/extras/doom/doom
DOOM_WAD ?= $(KERNEL_ROOT)/assets/doom.wad
LUA_SOURCE ?= $(KERNEL_ROOT)/extras/lua/src
LUA_BINARY ?= $(KERNEL_ROOT)/build/extras/lua/lua
ifneq (,$(wildcard config.mk))
include config.mk
endif
CFLAGS	= -m64 $(OPT) -Wall -Wextra -nostdlib -fno-builtin -nostartfiles -nodefaultlibs -ffreestanding -mcmodel=large -mno-red-zone -mno-mmx -mno-sse -mno-sse2  -I $(KERNEL_ROOT)  -g 
LD	= ld  -m elf_x86_64 
export CFLAGS 
export CC
export AS
export ASFLAGS
SUBDIRS = $(shell ls -d */)
KERNEL_OBJECT_DIRS = asm block fs init irq locks mm output sched timer utils
OBJ_FILES = $(shell find $(KERNEL_OBJECT_DIRS) -type f -name '*.o')

output/font.h: output/font.psf.gz scripts/psf-to-header.sh
	sh scripts/psf-to-header.sh $< $@

output/vga.o: output/font.h

all: kernel.img user

install: user
	$(MAKE) -C user install

utils: utils/llist.o
	$(MAKE) -C utils

asm: asm/asm.o asm/asm_calls.o
	$(MAKE) -C asm

block: block/ata.o 
	$(MAKE) -C block

fs: fs/fat.o fs/vfs.o fs/ramfs.o fs/pipe.o
	$(MAKE) -C fs

init: init/kernel.o init/loader.o init/tables.o init/dshell.o init/syscall.o
	$(MAKE) -C init

irq: irq/irq.o
	$(MAKE) -C irq

mm: mm/mm.o mm/paging.o mm/pmem.o mm/vmm.o
	$(MAKE) -C mm

sched: sched/sched.o sched/exec.o sched/ps.o
	$(MAKE) -C sched

output: output/output.o output/vga.o output/uart.o output/keyboard.o output/input.o output/framebuffer.o
	$(MAKE) -C output

timer: timer/pit.o timer/timer.o timer/rtc.o
	$(MAKE) -C timer

locks: locks/spinlock.o locks/mutex.o
	$(MAKE) -C locks

user:kernel.img
	$(MAKE) -C user

.PHONY: toolchain userland kedit doom doom-image lua image image-reset image-mount image-unmount image-copy

toolchain:
	$(MAKE) -C $(NEWLIB_BUILD)
	$(MAKE) -C $(NEWLIB_BUILD) install

userland:
	$(MAKE) -C $(CORE_SRC) NEWLIB_INSTALL=$(SYSROOT)
	@test -x $(PROGS_SRC)/progs/mv || $(MAKE) -C $(PROGS_SRC) mv
	$(MAKE) kedit
	mkdir -p $(USERLAND_STAGE)
	while IFS= read -r program; do \
		case "$$program" in ''|'#'*) continue;; esac; \
		if [ -f $(USERLAND_STAGE)/$$program ]; then :; \
		elif [ -f $(CORE_SRC)/$$program ]; then cp $(CORE_SRC)/$$program $(USERLAND_STAGE)/$$program; \
		elif [ -f $(PROGS_SRC)/progs/$$program ]; then cp $(PROGS_SRC)/progs/$$program $(USERLAND_STAGE)/$$program; \
		else echo "missing userland program: $$program" >&2; exit 1; fi; \
	done < image/manifest.txt

kedit: extras/kedit/kedit.c
	mkdir -p $(USERLAND_STAGE)
	$(CC) $(CFLAGS) -static -I $(SYSROOT)/include $< $(SYSROOT)/lib/libc.a $(SYSROOT)/lib/libm.a -o $(USERLAND_STAGE)/kedit

doom:
	SYSROOT=$(SYSROOT) scripts/build-doom.sh

doom-image:
	$(MAKE) doom
	$(MAKE) image-reset

lua:
	$(MAKE) -C $(LUA_SOURCE) clean
	$(MAKE) -C $(LUA_SOURCE) lua SYSROOT=$(SYSROOT)
	mkdir -p $(dir $(LUA_BINARY))
	cp $(LUA_SOURCE)/lua $(LUA_BINARY)

image: userland lua
	IMAGE_PATH=$(IMAGE) IMAGE_MOUNT=$(IMAGE_MOUNT) USERLAND_STAGE=$(USERLAND_STAGE) DOOM_BINARY=$(DOOM_BINARY) DOOM_WAD=$(DOOM_WAD) LUA_BINARY=$(LUA_BINARY) scripts/image-create.sh

image-reset: userland lua
	IMAGE_PATH=$(IMAGE) IMAGE_MOUNT=$(IMAGE_MOUNT) USERLAND_STAGE=$(USERLAND_STAGE) DOOM_BINARY=$(DOOM_BINARY) DOOM_WAD=$(DOOM_WAD) LUA_BINARY=$(LUA_BINARY) scripts/image-create.sh --reset

image-mount:
	IMAGE_PATH=$(IMAGE) IMAGE_MOUNT=$(IMAGE_MOUNT) scripts/image-mount.sh

image-unmount:
	IMAGE_PATH=$(IMAGE) IMAGE_MOUNT=$(IMAGE_MOUNT) scripts/image-unmount.sh

image-copy:
	@test -n "$(FILE)" || (echo "use: make image-copy FILE=/path/to/file" >&2; exit 2)
	@mountpoint -q $(IMAGE_MOUNT) || (echo "run make image-mount first" >&2; exit 1)
	sudo cp $(FILE) $(IMAGE_MOUNT)/$(notdir $(FILE))
    
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

test: kernel32.bin image
	qemu-system-x86_64 -m 64M -vga std -kernel kernel32.bin  -hda $(IMAGE) -serial stdio -rtc base=localtime

test-nox: kernel32.bin image
	qemu-system-x86_64 -m 4G -kernel kernel32.bin  -hda $(IMAGE) -display none -serial stdio -rtc base=localtime
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
