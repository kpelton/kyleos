CC	?= gcc
.DEFAULT_GOAL := all
OPT ?= -O3
AS = nasm
ASFLAGS = -f elf64
KERNEL_ROOT=$(shell pwd)
IMAGE ?= $(KERNEL_ROOT)/build/image/test-hd.img
IMAGE_MOUNT ?= $(KERNEL_ROOT)/build/image/mnt
USERLAND_STAGE ?= $(KERNEL_ROOT)/build/userland
KUI_LIBRARY ?= $(USERLAND_STAGE)/libkui.a
KUI_OBJECT ?= $(USERLAND_STAGE)/kui.o
KUI_CLIENT_OBJECT ?= $(USERLAND_STAGE)/kui_client.o
KUI_FILE_DIALOG_OBJECT ?= $(USERLAND_STAGE)/kui_file_dialog.o
KUI_DIALOG_OBJECT ?= $(USERLAND_STAGE)/kui_dialog.o
MINESWEEPER_WIDGET_OBJECT ?= $(USERLAND_STAGE)/minesweeper_widget.o
SOLITAIRE_WIDGET_OBJECT ?= $(USERLAND_STAGE)/solitaire_widget.o
CALCULATOR_WIDGET_OBJECT ?= $(USERLAND_STAGE)/calculator_widget.o
CORE_SRC ?= $(KERNEL_ROOT)/../../kyleos-userspace
PROGS_SRC ?= $(KERNEL_ROOT)/../../newlib-progs
NEWLIB_BUILD ?= $(KERNEL_ROOT)/../../newlib-build
SYSROOT ?= /tmp/z
DOOM_BINARY ?= $(KERNEL_ROOT)/build/extras/doom/doom
DOOM_WAD ?= /home/kyle/Downloads/DOOM.WAD
QUAKE_BINARY ?= $(KERNEL_ROOT)/build/extras/quake/quake
QUAKE_DATA_DIR ?= $(KERNEL_ROOT)/assets/quake/id1
DUKE3D_BINARY ?= $(KERNEL_ROOT)/build/extras/duke3d/duke3d
DUKE3D_GRP ?= $(KERNEL_ROOT)/assets/duke3d/duke3d.grp
LUA_SOURCE ?= $(KERNEL_ROOT)/extras/lua/src
LUA_BINARY ?= $(KERNEL_ROOT)/build/extras/lua/lua
LUA_INPUTS := $(wildcard $(LUA_SOURCE)/*.[ch]) $(LUA_SOURCE)/Makefile
BIBLE_TEXT ?= $(KERNEL_ROOT)/assets/bible.txt
SERIAL_OUTPUT ?= 1
GRUB_ISO_DIR := $(KERNEL_ROOT)/build/grub-iso
GRUB_CFG := $(KERNEL_ROOT)/boot/grub/grub.cfg

GRUB_ISO_DIR := $(KERNEL_ROOT)/boot/grub-iso
GRUB_CFG := $(KERNEL_ROOT)/boot/grub.cfg

ifneq (,$(wildcard config.mk))
include config.mk
endif
CFLAGS	= -m64 $(OPT) -Wall -Wextra -nostdlib -fno-builtin -nostartfiles -nodefaultlibs -ffreestanding -mcmodel=large -mno-red-zone -mno-mmx -mno-sse -mno-sse2  -I $(KERNEL_ROOT)  -g 
GUI_CFLAGS = $(filter-out -O%,$(CFLAGS)) -O2
ifeq ($(SERIAL_OUTPUT),0)
CFLAGS += -DKYLEOS_DISABLE_SERIAL_OUTPUT
endif
sched/sched.o: CFLAGS := $(filter-out -O%,$(CFLAGS)) -O0
LD	= ld  -m elf_x86_64 
export CFLAGS 
export CC
export AS
export ASFLAGS
SUBDIRS = $(shell ls -d */)
KERNEL_OBJECT_DIRS = asm block fs init irq locks mm output sched timer utils
OBJ_FILES = asm/asm.o asm/asm_calls.o \
	block/ata.o \
	fs/fat.o fs/ramfs.o fs/vfs.o fs/pipe.o \
	init/kernel.o init/tables.o init/dshell.o init/loader.o init/syscall.o \
	irq/irq.o \
	locks/spinlock.o locks/mutex.o \
	mm/paging.o mm/mm.o mm/pmem.o mm/vmm.o mm/mtrr.o \
	output/keyboard.o output/uart.o output/vga.o output/framebuffer.o \
	output/output.o output/input.o output/mouse.o \
	sched/sched.o sched/ps.o sched/exec.o \
	timer/timer.o timer/rtc.o timer/pit.o \
	utils/llist.o

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

output: output/output.o output/vga.o output/uart.o output/keyboard.o output/input.o output/framebuffer.o output/mouse.o
	$(MAKE) -C output

timer: timer/pit.o timer/timer.o timer/rtc.o
	$(MAKE) -C timer

locks: locks/spinlock.o locks/mutex.o
	$(MAKE) -C locks

user:kernel.img
	$(MAKE) -C user

.PHONY: toolchain userland userinit kui gui-assets desktop finder sysmon editor kterm minesweeper solitaire calculator kedit rm rmdir fstest gpfault waittest demandtest oomtest breakout cc bootstrap-cc doom doom-image quake quake-image duke3d duke3d-image image image-reset image-mount image-unmount image-copy iso iso-test

toolchain:
	$(MAKE) -C $(NEWLIB_BUILD)
	$(MAKE) -C $(NEWLIB_BUILD) install

userland:
	$(MAKE) -C $(CORE_SRC) NEWLIB_INSTALL=$(SYSROOT)
	@test -x $(PROGS_SRC)/progs/mv || $(MAKE) -C $(PROGS_SRC) mv
	$(MAKE) userinit desktop finder sysmon editor kterm minesweeper solitaire calculator kedit rm rmdir fstest gpfault waittest demandtest oomtest breakout cc
	mkdir -p $(USERLAND_STAGE)
	while IFS= read -r program; do \
		case "$$program" in ''|'#'*) continue;; esac; \
		if [ -f $(CORE_SRC)/$$program ]; then cp $(CORE_SRC)/$$program $(USERLAND_STAGE)/$$program; \
		elif [ -f $(PROGS_SRC)/progs/$$program ]; then cp $(PROGS_SRC)/progs/$$program $(USERLAND_STAGE)/$$program; \
		elif [ -f $(USERLAND_STAGE)/$$program ]; then :; \
		else echo "missing userland program: $$program" >&2; exit 1; fi; \
	done < image/manifest.txt

kedit: extras/kedit/kedit.c
	mkdir -p $(USERLAND_STAGE)
	$(CC) $(CFLAGS) -static -I $(SYSROOT)/include $< $(SYSROOT)/lib/libc.a $(SYSROOT)/lib/libm.a -o $(USERLAND_STAGE)/kedit

userinit: extras/init/init.c
	mkdir -p $(USERLAND_STAGE)
	$(CC) $(CFLAGS) -static -I $(SYSROOT)/include $< $(SYSROOT)/lib/libc.a $(SYSROOT)/lib/libm.a -o $(USERLAND_STAGE)/init

kui: $(KUI_LIBRARY)

gui-assets:
	scripts/build-gui-assets.sh

$(KUI_OBJECT): extras/kui/kui.c extras/kui/kui.h extras/kui/kui_protocol.h output/font.h include/uapi/framebuffer.h include/uapi/mouse.h
	mkdir -p $(USERLAND_STAGE)
	$(CC) $(GUI_CFLAGS) -I $(SYSROOT)/include -c extras/kui/kui.c -o $@

$(KUI_CLIENT_OBJECT): extras/kui/kui_client.c extras/kui/kui_client.h extras/kui/kui.h extras/kui/kui_protocol.h
	mkdir -p $(USERLAND_STAGE)
	$(CC) $(GUI_CFLAGS) -I $(SYSROOT)/include -c extras/kui/kui_client.c -o $@

$(KUI_FILE_DIALOG_OBJECT): extras/kui/kui_file_dialog.c extras/kui/kui_file_dialog.h extras/kui/kui.h extras/kui/kui_protocol.h
	mkdir -p $(USERLAND_STAGE)
	$(CC) $(GUI_CFLAGS) -I $(SYSROOT)/include -c extras/kui/kui_file_dialog.c -o $@

$(KUI_DIALOG_OBJECT): extras/kui/kui_dialog.c extras/kui/kui_dialog.h extras/kui/kui.h extras/kui/kui_protocol.h
	mkdir -p $(USERLAND_STAGE)
	$(CC) $(GUI_CFLAGS) -I $(SYSROOT)/include -c extras/kui/kui_dialog.c -o $@

$(KUI_LIBRARY): $(KUI_OBJECT) $(KUI_CLIENT_OBJECT) $(KUI_FILE_DIALOG_OBJECT) $(KUI_DIALOG_OBJECT)
	$(AR) rcs $@ $^

$(MINESWEEPER_WIDGET_OBJECT): extras/minesweeper/minesweeper_widget.c extras/minesweeper/minesweeper.h $(KUI_LIBRARY)
	mkdir -p $(USERLAND_STAGE)
	$(CC) $(GUI_CFLAGS) -I $(SYSROOT)/include -c extras/minesweeper/minesweeper_widget.c -o $@

$(SOLITAIRE_WIDGET_OBJECT): extras/solitaire/solitaire_widget.c extras/solitaire/solitaire.h $(KUI_LIBRARY)
	mkdir -p $(USERLAND_STAGE)
	$(CC) $(GUI_CFLAGS) -I $(SYSROOT)/include -c extras/solitaire/solitaire_widget.c -o $@

$(CALCULATOR_WIDGET_OBJECT): extras/calculator/calculator_widget.c extras/calculator/calculator.h $(KUI_LIBRARY)
	mkdir -p $(USERLAND_STAGE)
	$(CC) $(GUI_CFLAGS) -I $(SYSROOT)/include -c extras/calculator/calculator_widget.c -o $@

desktop: extras/desktop/desktop.c extras/desktop/shell.c extras/desktop/shell.h extras/kui/kui_protocol.h $(KUI_LIBRARY)
	mkdir -p $(USERLAND_STAGE)
	$(CC) $(GUI_CFLAGS) -static -I $(KERNEL_ROOT) -I $(SYSROOT)/include extras/desktop/desktop.c extras/desktop/shell.c $(KUI_LIBRARY) $(SYSROOT)/lib/libc.a $(SYSROOT)/lib/libm.a -o $(USERLAND_STAGE)/desktop

finder: extras/finder/finder.c extras/finder/finder_ipc.h $(KUI_LIBRARY)
	mkdir -p $(USERLAND_STAGE)
	$(CC) $(GUI_CFLAGS) -static -I $(KERNEL_ROOT) -I $(SYSROOT)/include $< $(KUI_LIBRARY) $(SYSROOT)/lib/libc.a $(SYSROOT)/lib/libm.a -o $(USERLAND_STAGE)/finder

sysmon: extras/sysmon/sysmon.c extras/sysmon/sysmon_ipc.h $(KUI_LIBRARY)
	mkdir -p $(USERLAND_STAGE)
	$(CC) $(GUI_CFLAGS) -static -I $(KERNEL_ROOT) -I $(SYSROOT)/include $< $(KUI_LIBRARY) $(SYSROOT)/lib/libc.a $(SYSROOT)/lib/libm.a -o $(USERLAND_STAGE)/sysmon

editor: extras/editor/editor.c extras/editor/editor_ipc.h $(KUI_LIBRARY)
	mkdir -p $(USERLAND_STAGE)
	$(CC) $(GUI_CFLAGS) -static -I $(KERNEL_ROOT) -I $(SYSROOT)/include $< $(KUI_LIBRARY) $(SYSROOT)/lib/libc.a $(SYSROOT)/lib/libm.a -o $(USERLAND_STAGE)/editor

kterm: extras/terminal/terminal.c extras/terminal/terminal_ipc.h $(KUI_LIBRARY)
	mkdir -p $(USERLAND_STAGE)
	$(CC) $(GUI_CFLAGS) -static -I $(KERNEL_ROOT) -I $(SYSROOT)/include $< $(KUI_LIBRARY) $(SYSROOT)/lib/libc.a $(SYSROOT)/lib/libm.a -o $(USERLAND_STAGE)/kterm

minesweeper: extras/minesweeper/minesweeper.c $(KUI_LIBRARY) $(MINESWEEPER_WIDGET_OBJECT)
	mkdir -p $(USERLAND_STAGE)
	$(CC) $(GUI_CFLAGS) -static -I $(KERNEL_ROOT) -I $(SYSROOT)/include $< $(MINESWEEPER_WIDGET_OBJECT) $(KUI_LIBRARY) $(SYSROOT)/lib/libc.a $(SYSROOT)/lib/libm.a -o $(USERLAND_STAGE)/minesweeper

solitaire: extras/solitaire/solitaire.c $(KUI_LIBRARY) $(SOLITAIRE_WIDGET_OBJECT)
	mkdir -p $(USERLAND_STAGE)
	$(CC) $(GUI_CFLAGS) -static -I $(KERNEL_ROOT) -I $(SYSROOT)/include $< $(SOLITAIRE_WIDGET_OBJECT) $(KUI_LIBRARY) $(SYSROOT)/lib/libc.a $(SYSROOT)/lib/libm.a -o $(USERLAND_STAGE)/solitaire

calculator: extras/calculator/calculator.c $(KUI_LIBRARY) $(CALCULATOR_WIDGET_OBJECT)
	mkdir -p $(USERLAND_STAGE)
	$(CC) $(GUI_CFLAGS) -static -I $(KERNEL_ROOT) -I $(SYSROOT)/include $< $(CALCULATOR_WIDGET_OBJECT) $(KUI_LIBRARY) $(SYSROOT)/lib/libc.a $(SYSROOT)/lib/libm.a -o $(USERLAND_STAGE)/calculator

rm: extras/rm/rm.c
	mkdir -p $(USERLAND_STAGE)
	$(CC) $(CFLAGS) -static -I $(SYSROOT)/include $< $(SYSROOT)/lib/libc.a $(SYSROOT)/lib/libm.a -o $(USERLAND_STAGE)/rm

rmdir: extras/rmdir/rmdir.c
	mkdir -p $(USERLAND_STAGE)
	$(CC) $(CFLAGS) -static -I $(SYSROOT)/include $< $(SYSROOT)/lib/libc.a $(SYSROOT)/lib/libm.a -o $(USERLAND_STAGE)/rmdir

fstest: extras/fstest/fstest.c
	mkdir -p $(USERLAND_STAGE)
	$(CC) $(CFLAGS) -static -I $(SYSROOT)/include $< $(SYSROOT)/lib/libc.a $(SYSROOT)/lib/libm.a -o $(USERLAND_STAGE)/fstest

gpfault: extras/gpfault/gpfault.c
	mkdir -p $(USERLAND_STAGE)
	$(CC) $(CFLAGS) -static -I $(SYSROOT)/include $< $(SYSROOT)/lib/libc.a $(SYSROOT)/lib/libm.a -o $(USERLAND_STAGE)/gpfault

waittest: extras/waittest/waittest.c
	mkdir -p $(USERLAND_STAGE)
	$(CC) $(CFLAGS) -static -I $(SYSROOT)/include $< $(SYSROOT)/lib/libc.a $(SYSROOT)/lib/libm.a -o $(USERLAND_STAGE)/waittest

demandtest: extras/demandtest/demandtest.c
	mkdir -p $(USERLAND_STAGE)
	$(CC) $(CFLAGS) -static -I $(SYSROOT)/include $< $(SYSROOT)/lib/libc.a $(SYSROOT)/lib/libm.a -o $(USERLAND_STAGE)/demandtest

oomtest: extras/oomtest/oomtest.c
	mkdir -p $(USERLAND_STAGE)
	$(CC) $(CFLAGS) -static -I $(SYSROOT)/include $< $(SYSROOT)/lib/libc.a $(SYSROOT)/lib/libm.a -o $(USERLAND_STAGE)/oomtest

breakout: extras/breakout/breakout.c
	mkdir -p $(USERLAND_STAGE)
	$(CC) $(CFLAGS) -static -I $(SYSROOT)/include $< $(SYSROOT)/lib/libc.a $(SYSROOT)/lib/libm.a -o $(USERLAND_STAGE)/breakout

cc:
	HOST_CC=$(CC) SYSROOT=$(SYSROOT) TCC_BINARY=$(USERLAND_STAGE)/cc MAKEFLAGS='$(MAKEFLAGS)' scripts/build-tinycc.sh

bootstrap-cc: cc
	@test -f build/extras/tinycc/src/tcc.c
	@test -f build/extras/tinycc/src/tccdefs_.h
	@test -f extras/tinycc-kyleos/compat.c
	@echo "TinyCC bootstrap compiler and sources are ready for make image-reset"

doom:
	SYSROOT=$(SYSROOT) scripts/build-doom.sh

doom-image:
	$(MAKE) doom
	$(MAKE) image-reset

quake:
	SYSROOT=$(SYSROOT) scripts/build-quake.sh

quake-image:
	$(MAKE) quake
	$(MAKE) image-reset

duke3d:
	SYSROOT=$(SYSROOT) scripts/build-duke3d.sh

duke3d-image:
	$(MAKE) duke3d
	$(MAKE) image-reset

lua: $(LUA_BINARY)

$(LUA_BINARY): $(LUA_INPUTS)
	$(MAKE) -C $(LUA_SOURCE) clean
	$(MAKE) -C $(LUA_SOURCE) lua SYSROOT=$(SYSROOT)
	mkdir -p $(dir $(LUA_BINARY))
	cp $(LUA_SOURCE)/lua $(LUA_BINARY)

image: userland lua
	IMAGE_PATH=$(IMAGE) IMAGE_MOUNT=$(IMAGE_MOUNT) USERLAND_STAGE=$(USERLAND_STAGE) CORE_SRC=$(CORE_SRC) PROGS_SRC=$(PROGS_SRC) DOOM_BINARY=$(DOOM_BINARY) DOOM_WAD=$(DOOM_WAD) QUAKE_BINARY=$(QUAKE_BINARY) QUAKE_DATA_DIR=$(QUAKE_DATA_DIR) DUKE3D_BINARY=$(DUKE3D_BINARY) DUKE3D_GRP=$(DUKE3D_GRP) LUA_BINARY=$(LUA_BINARY) BIBLE_TEXT=$(BIBLE_TEXT) TCC_RUNTIME=$(KERNEL_ROOT)/build/extras/tinycc/root scripts/image-create.sh

image-reset: userland lua
	IMAGE_PATH=$(IMAGE) IMAGE_MOUNT=$(IMAGE_MOUNT) USERLAND_STAGE=$(USERLAND_STAGE) CORE_SRC=$(CORE_SRC) PROGS_SRC=$(PROGS_SRC) DOOM_BINARY=$(DOOM_BINARY) DOOM_WAD=$(DOOM_WAD) QUAKE_BINARY=$(QUAKE_BINARY) QUAKE_DATA_DIR=$(QUAKE_DATA_DIR) DUKE3D_BINARY=$(DUKE3D_BINARY) DUKE3D_GRP=$(DUKE3D_GRP) LUA_BINARY=$(LUA_BINARY) BIBLE_TEXT=$(BIBLE_TEXT) TCC_RUNTIME=$(KERNEL_ROOT)/build/extras/tinycc/root scripts/image-create.sh --reset

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

kernel.bin: $(OBJ_FILES)
	$(LD) -T linker.ld -o kernel.bin $(OBJ_FILES) 

kernel32.bin: kernel.bin
	objcopy   -I elf64-x86-64 -O elf32-i386   kernel.bin kernel32.bin

kernel.img: kernel32.bin

test: kernel32.bin image
	qemu-system-x86_64 -m 64M -vga std -kernel kernel32.bin  -hda $(IMAGE) -serial stdio -rtc base=localtime

test-nox: kernel32.bin image
	qemu-system-x86_64 -m 4G -kernel kernel32.bin  -hda $(IMAGE) -display none -serial stdio -rtc base=localtime
test-log: kernel32.bin
	qemu-system-x86_64 -m 8G -kernel kernel32.bin -d int,cpu_reset -hda test-hd.img -serial stdio 2>log
kvm-test: kernel32.bin
	qemu-system-x86_64 -m 8G -kernel kernel32.bin -d int,cpu_reset -hda $(IMAGE) -serial stdio -enable-kvm 2>log

test-c: kernel32.bin
	qemu-system-x86_64 -m 8G -kernel kernel32.bin -d int,cpu_reset -hda test-hd.img -display none -serial stdio 2>log
debug: kernel32.bin
	qemu-system-x86_64 -m 8G -kernel kernel32.bin -hda test-hd.img -serial stdio -s -S
debug-nox: kernel32.bin
	qemu-system-x86_64 -m 8G -display none -kernel kernel32.bin -hda test-hd.img -serial stdio -s -S

gdb: kernel.bin
	gdb -ex "target remote localhost:1234" kernel.bin


iso: kernel.bin $(GRUB_CFG)
	rm -rf $(GRUB_ISO_DIR)
	mkdir -p $(GRUB_ISO_DIR)/boot/grub
	cp kernel.bin $(GRUB_ISO_DIR)/boot/kernel.bin
	cp $(GRUB_CFG) $(GRUB_ISO_DIR)/boot/grub/grub.cfg
	grub-mkrescue -o bootable.iso $(GRUB_ISO_DIR)

iso-test: iso
	qemu-system-x86_64 \
		-m 60M \
		-cdrom bootable.iso \
		-serial stdio

test-suite:kernel.bin
	bash test.sh 2>/dev/null
