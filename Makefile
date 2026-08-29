CC	?= gcc
.DEFAULT_GOAL := all
OPT ?= -O3
AS = nasm
ASFLAGS = -f elf64
KERNEL_ROOT=$(shell pwd)
IMAGE ?= $(KERNEL_ROOT)/build/image/test-hd.img
IMAGE_MOUNT ?= $(KERNEL_ROOT)/build/image/mnt
TEST_IMAGE ?= $(KERNEL_ROOT)/build/image/test-suite.img
TEST_IMAGE_MOUNT ?= $(KERNEL_ROOT)/build/image/test-suite-mnt
TEST_TIMEOUT ?= 180
QEMU ?= qemu-system-x86_64
# Extra options are intentionally overridable: e.g. `make test QEMU_OPTS="-cpu host -enable-kvm"`.
QEMU_OPTS ?= -rtc base=localtime
USERLAND_STAGE ?= $(KERNEL_ROOT)/build/userland
KUI_LIBRARY ?= $(USERLAND_STAGE)/libkui.a
KUI_OBJECT ?= $(USERLAND_STAGE)/kui.o
KUI_CLIENT_OBJECT ?= $(USERLAND_STAGE)/kui_client.o
KUI_FILE_DIALOG_OBJECT ?= $(USERLAND_STAGE)/kui_file_dialog.o
KUI_DIALOG_OBJECT ?= $(USERLAND_STAGE)/kui_dialog.o
KUI_MENU_OBJECT ?= $(USERLAND_STAGE)/kui_menu.o
KUI_TEXT_SELECTION_OBJECT ?= $(USERLAND_STAGE)/kui_text_selection.o
KUI_STYLE_OBJECT ?= $(USERLAND_STAGE)/kui_style.o
KUI_STYLE_WIN95_OBJECT ?= $(USERLAND_STAGE)/kui_style_win95.o
KUI_STYLE_AQUA_OBJECT ?= $(USERLAND_STAGE)/kui_style_aqua.o
MINESWEEPER_WIDGET_OBJECT ?= $(USERLAND_STAGE)/minesweeper_widget.o
SOLITAIRE_WIDGET_OBJECT ?= $(USERLAND_STAGE)/solitaire_widget.o
CALCULATOR_WIDGET_OBJECT ?= $(USERLAND_STAGE)/calculator_widget.o
CORE_SRC ?= $(KERNEL_ROOT)/userspace/core
PROGS_SRC ?= $(KERNEL_ROOT)/userspace/programs
NEWLIB_BUILD ?= $(KERNEL_ROOT)/toolchain/newlib-build
SYSROOT ?= $(KERNEL_ROOT)/build/sysroot
DOOM_BINARY ?= $(KERNEL_ROOT)/build/extras/doom/doom
DOOM_AQUA_BINARY ?= $(KERNEL_ROOT)/build/extras/doom-aqua/doom-aqua
DOOM_WAD ?= $(HOME)/Downloads/DOOM.WAD
QUAKE_BINARY ?= $(KERNEL_ROOT)/build/extras/quake/quake
QUAKE_AQUA_BINARY ?= $(KERNEL_ROOT)/build/extras/quake-aqua/quake-aqua
QUAKE_DATA_DIR ?= $(KERNEL_ROOT)/assets/quake/id1
DUKE3D_BINARY ?= $(KERNEL_ROOT)/build/extras/duke3d/duke3d
DUKE3D_GRP ?= $(KERNEL_ROOT)/assets/duke3d/duke3d.grp
LUA_SOURCE ?= $(KERNEL_ROOT)/extras/lua/src
LUA_BINARY ?= $(KERNEL_ROOT)/build/extras/lua/lua
LUA_INPUTS := $(wildcard $(LUA_SOURCE)/*.[ch]) $(LUA_SOURCE)/Makefile
BUSYBOX_BINARY ?= $(KERNEL_ROOT)/build/userland/busybox
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
USER_CFLAGS = -m64 $(OPT) -Wall -Wextra -nostdlib -fno-builtin -nostartfiles -nodefaultlibs -ffreestanding -mcmodel=large -O3  -I $(KERNEL_ROOT)   -msse4.2
GUI_CFLAGS = $(filter-out -O%,$(USER_CFLAGS)) -O3 -msse4.2

KSHEET_CFLAGS = $(GUI_CFLAGS)
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
KERNEL_OBJECT_DIRS = asm block drivers fs init irq locks mm output sched timer utils
OBJ_FILES = asm/asm.o asm/asm_calls.o \
	block/ata.o \
	drivers/pci.o \
	fs/fat.o fs/ramfs.o fs/vfs.o fs/pipe.o fs/pty.o \
	init/kernel.o init/tables.o init/dshell.o init/loader.o init/syscall.o \
	irq/irq.o \
	locks/spinlock.o locks/mutex.o \
	mm/paging.o mm/mm.o mm/pmem.o mm/vmm.o mm/mtrr.o \
	output/keyboard.o output/uart.o output/vga.o output/vt100.o output/framebuffer.o \
	output/output.o output/input.o output/tty.o output/mouse.o \
	sched/sched.o sched/ps.o sched/exec.o sched/signal.o \
	timer/timer.o timer/rtc.o timer/pit.o \
	utils/llist.o

output/font.h: output/font.psf.gz scripts/psf-to-header.sh
	sh scripts/psf-to-header.sh $< $@

output/vga.o: output/font.h

fs/pipe.o: fs/pipe.h
fs/pty.o: fs/pty.h

all: kernel.img user

install: user
	$(MAKE) -C user install

utils: utils/llist.o
	$(MAKE) -C utils

asm: asm/asm.o asm/asm_calls.o
	$(MAKE) -C asm

block: block/ata.o 
	$(MAKE) -C block

drivers: drivers/pci.o
	$(MAKE) -C drivers

fs: fs/fat.o fs/vfs.o fs/ramfs.o fs/pipe.o fs/pty.o
	$(MAKE) -C fs

init: init/kernel.o init/loader.o init/tables.o init/dshell.o init/syscall.o
	$(MAKE) -C init

irq: irq/irq.o
	$(MAKE) -C irq

mm: mm/mm.o mm/paging.o mm/pmem.o mm/vmm.o
	$(MAKE) -C mm

sched: sched/sched.o sched/exec.o sched/ps.o sched/signal.o
	$(MAKE) -C sched

output: output/output.o output/vga.o output/vt100.o output/uart.o output/keyboard.o output/input.o output/tty.o output/framebuffer.o output/mouse.o
	$(MAKE) -C output

timer: timer/pit.o timer/timer.o timer/rtc.o
	$(MAKE) -C timer

locks: locks/spinlock.o locks/mutex.o
	$(MAKE) -C locks

user:kernel.img
	$(MAKE) -C user

.PHONY: toolchain userland userinit nushell ps kill kui gui-assets desktop kuid win95-shell aqua aqua-shell controlcenter finder sysmon editor notes writer cide kterm ktermtest ksheet minesweeper solitaire calculator paint citybuilder tetris flappy kylekart fps help parallel-writers kedit rm rmdir alltest fstest gpfault kmsgtest waittest pipetest polltest signaltest ttytest ptytest demandtest oomtest breakout cc bootstrap-cc test-alltest-runner test-c-syntax test-city-sim test-desktop-config test-controlcenter-ui test-writer-document test-writer-ui test-kui-scaled-text test-paint-brush test-tetris test-tetris-ui test-fps test-fps-ui test-doom-aqua test-quake-aqua test-ksheet test-ksheet-ui test-kui-styles test-kui-shells test-kui-text-selection test-kui-context-menu test-editor-clipboard test-terminal-vt100 test-terminal-async test-help-ui test-unit test-e2e-run test-e2e test-suite test-busybox-run test-busybox busybox busybox-image doom doom-image doom-aqua doom-aqua-image quake quake-image quake-aqua quake-aqua-image refresh-kui-extras duke3d duke3d-image image image-reset image-mount image-unmount image-copy iso iso-test

HOST_TEST_TARGETS = \
	test-alltest-runner test-c-syntax test-city-sim \
	test-desktop-config test-controlcenter-ui \
	test-writer-document test-writer-ui test-kui-scaled-text \
	test-paint-brush test-tetris test-tetris-ui test-fps test-fps-ui \
	test-doom-aqua test-quake-aqua test-ksheet test-ksheet-ui \
	test-kui-styles test-kui-text-selection test-kui-context-menu \
	test-editor-clipboard test-terminal-vt100 test-terminal-async \
	test-help-ui test-kui-shells

toolchain:
	$(MAKE) -C $(NEWLIB_BUILD) prefix=$(SYSROOT) exec_prefix=$(SYSROOT) \
		CFLAGS="-g -fno-stack-protector -DSIGNAL_PROVIDED" DESTDIR= all
	$(MAKE) -C $(NEWLIB_BUILD) prefix=$(SYSROOT) exec_prefix=$(SYSROOT) \
		CFLAGS="-g -fno-stack-protector -DSIGNAL_PROVIDED" DESTDIR= install

userland:
	$(MAKE) -C $(CORE_SRC) NEWLIB_INSTALL=$(SYSROOT)
	$(MAKE) -C $(PROGS_SRC) NEWLIB_INSTALL=$(SYSROOT) cp mv
	$(MAKE) userinit nushell ps kill desktop kuid win95-shell aqua aqua-shell controlcenter finder sysmon editor notes writer cide kterm ktermtest ksheet minesweeper solitaire calculator paint citybuilder tetris flappy kylekart fps help parallel-writers kedit rm rmdir alltest fstest gpfault kmsgtest waittest pipetest polltest signaltest ttytest ptytest demandtest oomtest breakout cc
	mkdir -p $(USERLAND_STAGE)
	cp $(PROGS_SRC)/progs/cp $(USERLAND_STAGE)/cp
	cp $(PROGS_SRC)/progs/mv $(USERLAND_STAGE)/mv
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

userinit: extras/init/init.c
	mkdir -p $(USERLAND_STAGE)
	$(CC) $(CFLAGS) -static -I $(SYSROOT)/include $< $(SYSROOT)/lib/libc.a $(SYSROOT)/lib/libm.a -o $(USERLAND_STAGE)/init

nushell: extras/shell/nushell.c
	mkdir -p $(USERLAND_STAGE)
	$(CC) $(CFLAGS) -static -I $(SYSROOT)/include $< $(SYSROOT)/lib/libc.a -o $(USERLAND_STAGE)/nushell

ps: extras/shell/ps.c
	mkdir -p $(USERLAND_STAGE)
	$(CC) $(CFLAGS) -static -I $(SYSROOT)/include $< $(SYSROOT)/lib/libc.a -o $(USERLAND_STAGE)/ps

kill: extras/shell/kill.c
	mkdir -p $(USERLAND_STAGE)
	$(CC) $(CFLAGS) -static -I $(SYSROOT)/include $< $(SYSROOT)/lib/libc.a -o $(USERLAND_STAGE)/kill

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

$(KUI_MENU_OBJECT): extras/kui/kui_menu.c extras/kui/kui_menu.h extras/kui/kui.h extras/kui/kui_protocol.h
	mkdir -p $(USERLAND_STAGE)
	$(CC) $(GUI_CFLAGS) -I $(SYSROOT)/include -c extras/kui/kui_menu.c -o $@

$(KUI_TEXT_SELECTION_OBJECT): extras/kui/kui_text_selection.c extras/kui/kui_text_selection.h extras/kui/kui_protocol.h
	mkdir -p $(USERLAND_STAGE)
	$(CC) $(GUI_CFLAGS) -I $(SYSROOT)/include -c extras/kui/kui_text_selection.c -o $@

$(KUI_STYLE_OBJECT): extras/kui/kui_style.c extras/kui/kui_style.h
	mkdir -p $(USERLAND_STAGE)
	$(CC) $(GUI_CFLAGS) -I $(SYSROOT)/include -c $< -o $@

$(KUI_STYLE_WIN95_OBJECT): extras/kui/kui_style_win95.c extras/kui/kui_style.h
	mkdir -p $(USERLAND_STAGE)
	$(CC) $(GUI_CFLAGS) -I $(SYSROOT)/include -c $< -o $@

$(KUI_STYLE_AQUA_OBJECT): extras/kui/kui_style_aqua.c extras/kui/kui_style.h
	mkdir -p $(USERLAND_STAGE)
	$(CC) $(GUI_CFLAGS) -I $(SYSROOT)/include -c $< -o $@

$(KUI_LIBRARY): $(KUI_OBJECT) $(KUI_CLIENT_OBJECT) $(KUI_FILE_DIALOG_OBJECT) $(KUI_DIALOG_OBJECT) $(KUI_MENU_OBJECT) $(KUI_TEXT_SELECTION_OBJECT) $(KUI_STYLE_OBJECT) $(KUI_STYLE_WIN95_OBJECT) $(KUI_STYLE_AQUA_OBJECT)
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

desktop: extras/desktop/desktop.c
	mkdir -p $(USERLAND_STAGE)
	$(CC) $(CFLAGS) -static -I $(SYSROOT)/include $< $(SYSROOT)/lib/libc.a $(SYSROOT)/lib/libm.a -o $(USERLAND_STAGE)/desktop

kuid: extras/kuid/kuid.c extras/desktop/shell.c extras/desktop/shell.h extras/desktop/desktop_config.c extras/desktop/desktop_config.h extras/kuid/shell_protocol.h $(KUI_LIBRARY)
	mkdir -p $(USERLAND_STAGE)
	$(CC) $(GUI_CFLAGS) -static -I $(KERNEL_ROOT) -I $(SYSROOT)/include extras/kuid/kuid.c extras/desktop/shell.c extras/desktop/desktop_config.c $(KUI_LIBRARY) $(SYSROOT)/lib/libc.a $(SYSROOT)/lib/libm.a -o $(USERLAND_STAGE)/kuid

win95-shell: extras/kuid/win95_shell.c extras/kuid/shell_protocol.h $(KUI_LIBRARY)
	mkdir -p $(USERLAND_STAGE)
	$(CC) $(GUI_CFLAGS) -static -I $(KERNEL_ROOT) -I $(SYSROOT)/include $< $(KUI_LIBRARY) $(SYSROOT)/lib/libc.a $(SYSROOT)/lib/libm.a -o $(USERLAND_STAGE)/win95-shell

aqua: extras/kuid/aqua.c
	mkdir -p $(USERLAND_STAGE)
	$(CC) $(CFLAGS) -static -I $(SYSROOT)/include $< $(SYSROOT)/lib/libc.a $(SYSROOT)/lib/libm.a -o $(USERLAND_STAGE)/aqua

aqua-shell: extras/kuid/aqua_shell.c extras/kuid/shell_protocol.h $(KUI_LIBRARY)
	mkdir -p $(USERLAND_STAGE)
	$(CC) $(GUI_CFLAGS) -static -I $(KERNEL_ROOT) -I $(SYSROOT)/include $< $(KUI_LIBRARY) $(SYSROOT)/lib/libc.a $(SYSROOT)/lib/libm.a -o $(USERLAND_STAGE)/aqua-shell

controlcenter: extras/controlcenter/controlcenter.c extras/desktop/desktop_config.c extras/desktop/desktop_config.h $(KUI_LIBRARY)
	mkdir -p $(USERLAND_STAGE)
	$(CC) $(GUI_CFLAGS) -static -I $(KERNEL_ROOT) -I $(SYSROOT)/include extras/controlcenter/controlcenter.c extras/desktop/desktop_config.c $(KUI_LIBRARY) $(SYSROOT)/lib/libc.a $(SYSROOT)/lib/libm.a -o $(USERLAND_STAGE)/controlcenter

finder: extras/finder/finder.c extras/finder/finder_ipc.h $(KUI_LIBRARY)
	mkdir -p $(USERLAND_STAGE)
	$(CC) $(GUI_CFLAGS) -static -I $(KERNEL_ROOT) -I $(SYSROOT)/include $< $(KUI_LIBRARY) $(SYSROOT)/lib/libc.a $(SYSROOT)/lib/libm.a -o $(USERLAND_STAGE)/finder

sysmon: extras/sysmon/sysmon.c extras/sysmon/sysmon_ipc.h $(KUI_LIBRARY)
	mkdir -p $(USERLAND_STAGE)
	$(CC) $(GUI_CFLAGS) -static -I $(KERNEL_ROOT) -I $(SYSROOT)/include $< $(KUI_LIBRARY) $(SYSROOT)/lib/libc.a $(SYSROOT)/lib/libm.a -o $(USERLAND_STAGE)/sysmon

editor: extras/editor/editor.c extras/editor/editor_ipc.h $(KUI_LIBRARY)
	mkdir -p $(USERLAND_STAGE)
	$(CC) $(GUI_CFLAGS) -static -I $(KERNEL_ROOT) -I $(SYSROOT)/include $< $(KUI_LIBRARY) $(SYSROOT)/lib/libc.a $(SYSROOT)/lib/libm.a -o $(USERLAND_STAGE)/editor

notes: extras/editor/editor.c extras/editor/editor_ipc.h $(KUI_LIBRARY)
	mkdir -p $(USERLAND_STAGE)
	$(CC) $(GUI_CFLAGS) -DNOTES_APP -static -I $(KERNEL_ROOT) -I $(SYSROOT)/include $< $(KUI_LIBRARY) $(SYSROOT)/lib/libc.a $(SYSROOT)/lib/libm.a -o $(USERLAND_STAGE)/notes

writer: extras/wordprocessor/wordprocessor.c extras/wordprocessor/writer_document.c extras/wordprocessor/writer_document.h $(KUI_LIBRARY)
	mkdir -p $(USERLAND_STAGE)
	$(CC) $(GUI_CFLAGS) -static -I $(KERNEL_ROOT) -I $(SYSROOT)/include extras/wordprocessor/wordprocessor.c extras/wordprocessor/writer_document.c $(KUI_LIBRARY) $(SYSROOT)/lib/libc.a $(SYSROOT)/lib/libm.a -o $(USERLAND_STAGE)/writer

cide: extras/cide/cide.c extras/editor/editor.c extras/editor/editor_ipc.h extras/editor/c_syntax.c extras/editor/c_syntax.h $(KUI_LIBRARY)
	mkdir -p $(USERLAND_STAGE)
	$(CC) $(GUI_CFLAGS) -static -I $(KERNEL_ROOT) -I $(SYSROOT)/include extras/cide/cide.c extras/editor/c_syntax.c $(KUI_LIBRARY) $(SYSROOT)/lib/libc.a $(SYSROOT)/lib/libm.a -o $(USERLAND_STAGE)/cide

kterm: extras/terminal/terminal.c extras/terminal/terminal_ipc.h extras/terminal/vt100.c extras/terminal/vt100.h $(KUI_LIBRARY)
	mkdir -p $(USERLAND_STAGE)
	$(CC) $(GUI_CFLAGS) -static -I $(KERNEL_ROOT) -I $(SYSROOT)/include extras/terminal/terminal.c extras/terminal/vt100.c $(KUI_LIBRARY) $(SYSROOT)/lib/libc.a $(SYSROOT)/lib/libm.a -o $(USERLAND_STAGE)/kterm

ktermtest: tests/test-terminal-async.c $(KUI_LIBRARY)
	mkdir -p $(USERLAND_STAGE)
	$(CC) $(GUI_CFLAGS) -DKYLEOS_GUEST -static -I $(KERNEL_ROOT) -I $(SYSROOT)/include $< $(KUI_LIBRARY) $(SYSROOT)/lib/libc.a $(SYSROOT)/lib/libm.a -o $(USERLAND_STAGE)/ktermtest

ksheet: extras/ksheet/ksheet.c extras/ksheet/ksheet_model.c extras/ksheet/ksheet_model.h $(KUI_LIBRARY)
	mkdir -p $(USERLAND_STAGE)
	$(CC) $(KSHEET_CFLAGS) -static -I $(KERNEL_ROOT) -I $(SYSROOT)/include extras/ksheet/ksheet.c extras/ksheet/ksheet_model.c $(KUI_LIBRARY) $(SYSROOT)/lib/libc.a $(SYSROOT)/lib/libm.a -o $(USERLAND_STAGE)/ksheet

minesweeper: extras/minesweeper/minesweeper.c $(KUI_LIBRARY) $(MINESWEEPER_WIDGET_OBJECT)
	mkdir -p $(USERLAND_STAGE)
	$(CC) $(GUI_CFLAGS) -static -I $(KERNEL_ROOT) -I $(SYSROOT)/include $< $(MINESWEEPER_WIDGET_OBJECT) $(KUI_LIBRARY) $(SYSROOT)/lib/libc.a $(SYSROOT)/lib/libm.a -o $(USERLAND_STAGE)/minesweeper

solitaire: extras/solitaire/solitaire.c $(KUI_LIBRARY) $(SOLITAIRE_WIDGET_OBJECT)
	mkdir -p $(USERLAND_STAGE)
	$(CC) $(GUI_CFLAGS) -static -I $(KERNEL_ROOT) -I $(SYSROOT)/include $< $(SOLITAIRE_WIDGET_OBJECT) $(KUI_LIBRARY) $(SYSROOT)/lib/libc.a $(SYSROOT)/lib/libm.a -o $(USERLAND_STAGE)/solitaire

calculator: extras/calculator/calculator.c $(KUI_LIBRARY) $(CALCULATOR_WIDGET_OBJECT)
	mkdir -p $(USERLAND_STAGE)
	$(CC) $(GUI_CFLAGS) -static -I $(KERNEL_ROOT) -I $(SYSROOT)/include $< $(CALCULATOR_WIDGET_OBJECT) $(KUI_LIBRARY) $(SYSROOT)/lib/libc.a $(SYSROOT)/lib/libm.a -o $(USERLAND_STAGE)/calculator

paint: extras/paint/paint.c extras/paint/paint_brush.c extras/paint/paint_brush.h $(KUI_LIBRARY)
	mkdir -p $(USERLAND_STAGE)
	$(CC) $(GUI_CFLAGS) -static -I $(KERNEL_ROOT) -I $(SYSROOT)/include extras/paint/paint.c extras/paint/paint_brush.c $(KUI_LIBRARY) $(SYSROOT)/lib/libc.a $(SYSROOT)/lib/libm.a -o $(USERLAND_STAGE)/paint

citybuilder: extras/citybuilder/citybuilder.c extras/citybuilder/city_sim.c extras/citybuilder/city_sim.h $(KUI_LIBRARY)
	mkdir -p $(USERLAND_STAGE)
	$(CC) $(GUI_CFLAGS) -static -I $(KERNEL_ROOT) -I $(SYSROOT)/include extras/citybuilder/citybuilder.c extras/citybuilder/city_sim.c $(KUI_LIBRARY) $(SYSROOT)/lib/libc.a $(SYSROOT)/lib/libm.a -o $(USERLAND_STAGE)/citybuilder

tetris: extras/tetris/tetris.c extras/tetris/tetris_game.c extras/tetris/tetris_game.h $(KUI_LIBRARY)
	mkdir -p $(USERLAND_STAGE)
	$(CC) $(GUI_CFLAGS) -static -I $(KERNEL_ROOT) -I $(SYSROOT)/include extras/tetris/tetris.c extras/tetris/tetris_game.c $(KUI_LIBRARY) $(SYSROOT)/lib/libc.a $(SYSROOT)/lib/libm.a -o $(USERLAND_STAGE)/tetris

flappy: extras/flappy/flappy.c $(KUI_LIBRARY)
	mkdir -p $(USERLAND_STAGE)
	$(CC) $(GUI_CFLAGS) -static -I $(KERNEL_ROOT) -I $(SYSROOT)/include $< $(KUI_LIBRARY) $(SYSROOT)/lib/libc.a $(SYSROOT)/lib/libm.a -o $(USERLAND_STAGE)/flappy

kylekart: extras/kylekart/kylekart.c $(KUI_LIBRARY)
	mkdir -p $(USERLAND_STAGE)
	$(CC) $(GUI_CFLAGS) -static -I $(KERNEL_ROOT) -I $(SYSROOT)/include $< $(KUI_LIBRARY) $(SYSROOT)/lib/libc.a $(SYSROOT)/lib/libm.a -o $(USERLAND_STAGE)/kylekart

fps: extras/fps/fps.c extras/fps/fps_game.c extras/fps/fps_game.h $(KUI_LIBRARY)
	mkdir -p $(USERLAND_STAGE)
	$(CC) $(GUI_CFLAGS) -static -I $(KERNEL_ROOT) -I $(SYSROOT)/include extras/fps/fps.c extras/fps/fps_game.c $(KUI_LIBRARY) $(SYSROOT)/lib/libc.a $(SYSROOT)/lib/libm.a -o $(USERLAND_STAGE)/fps

help: extras/help/help.c $(KUI_LIBRARY)
	mkdir -p $(USERLAND_STAGE)
	$(CC) $(GUI_CFLAGS) -static -I $(KERNEL_ROOT) -I $(SYSROOT)/include $< $(KUI_LIBRARY) $(SYSROOT)/lib/libc.a $(SYSROOT)/lib/libm.a -o $(USERLAND_STAGE)/help

parallel-writers: extras/parallel-writer/parallel_writer.c $(KUI_LIBRARY)
	mkdir -p $(USERLAND_STAGE)
	$(CC) $(GUI_CFLAGS) -DWRITER_SIDE=0 -static -I $(KERNEL_ROOT) -I $(SYSROOT)/include $< $(KUI_LIBRARY) $(SYSROOT)/lib/libc.a $(SYSROOT)/lib/libm.a -o $(USERLAND_STAGE)/parallel-writer-a
	$(CC) $(GUI_CFLAGS) -DWRITER_SIDE=1 -static -I $(KERNEL_ROOT) -I $(SYSROOT)/include $< $(KUI_LIBRARY) $(SYSROOT)/lib/libc.a $(SYSROOT)/lib/libm.a -o $(USERLAND_STAGE)/parallel-writer-b

rm: extras/rm/rm.c
	mkdir -p $(USERLAND_STAGE)
	$(CC) $(CFLAGS) -static -I $(SYSROOT)/include $< $(SYSROOT)/lib/libc.a $(SYSROOT)/lib/libm.a -o $(USERLAND_STAGE)/rm

rmdir: extras/rmdir/rmdir.c
	mkdir -p $(USERLAND_STAGE)
	$(CC) $(CFLAGS) -static -I $(SYSROOT)/include $< $(SYSROOT)/lib/libc.a $(SYSROOT)/lib/libm.a -o $(USERLAND_STAGE)/rmdir

alltest: extras/alltest/alltest.c
	mkdir -p $(USERLAND_STAGE)
	$(CC) $(CFLAGS) -static -I $(SYSROOT)/include $< $(SYSROOT)/lib/libc.a $(SYSROOT)/lib/libm.a -o $(USERLAND_STAGE)/alltest

test-alltest-runner:
	bash tests/test-alltest-runner.sh

test-c-syntax:
	$(CC) -std=c11 -Wall -Wextra -Werror -I $(KERNEL_ROOT) \
		tests/test-c-syntax.c extras/editor/c_syntax.c \
		-o /tmp/os3-test-c-syntax
	/tmp/os3-test-c-syntax

test-city-sim:
	$(CC) -std=c11 -Wall -Wextra -Werror -I $(KERNEL_ROOT) \
		tests/test-city-sim.c extras/citybuilder/city_sim.c \
		-o /tmp/os3-test-city-sim
	/tmp/os3-test-city-sim

test-desktop-config:
	$(CC) -std=c11 -Wall -Wextra -Werror -I $(KERNEL_ROOT) \
		tests/test-desktop-config.c extras/desktop/desktop_config.c \
		-o /tmp/os3-test-desktop-config
	/tmp/os3-test-desktop-config

test-controlcenter-ui:
	$(CC) -std=gnu11 -Wall -Wextra -Werror -I $(KERNEL_ROOT) \
		-DDESKTOP_CONFIG_PATH='"/tmp/os3-control-center.cfg"' \
		extras/controlcenter/controlcenter.c extras/desktop/desktop_config.c \
		extras/kui/kui.c extras/kui/kui_client.c extras/kui/kui_style.c \
		extras/kui/kui_style_win95.c extras/kui/kui_style_aqua.c \
		-o /tmp/os3-controlcenter-host
	$(CC) -std=gnu11 -Wall -Wextra -Werror -I $(KERNEL_ROOT) \
		-DDESKTOP_CONFIG_PATH='"/tmp/os3-control-center.cfg"' \
		tests/test-controlcenter-ui.c extras/desktop/desktop_config.c \
		extras/kui/kui.c extras/kui/kui_style.c \
		extras/kui/kui_style_win95.c extras/kui/kui_style_aqua.c \
		-o /tmp/os3-test-controlcenter-ui
	/tmp/os3-test-controlcenter-ui /tmp/os3-controlcenter-host

test-writer-document:
	$(CC) -std=c11 -Wall -Wextra -Werror -I $(KERNEL_ROOT) \
		tests/test-writer-document.c extras/wordprocessor/writer_document.c \
		-o /tmp/os3-test-writer-document
	/tmp/os3-test-writer-document

test-writer-ui:
	$(CC) -std=gnu11 -Wall -Wextra -Werror -I $(KERNEL_ROOT) \
		extras/wordprocessor/wordprocessor.c \
		extras/wordprocessor/writer_document.c extras/kui/kui.c \
		extras/kui/kui_client.c extras/kui/kui_file_dialog.c \
		extras/kui/kui_dialog.c extras/kui/kui_text_selection.c \
		extras/kui/kui_style.c extras/kui/kui_style_win95.c \
		extras/kui/kui_style_aqua.c -o /tmp/os3-writer-host
	$(CC) -std=gnu11 -Wall -Wextra -Werror -I $(KERNEL_ROOT) \
		tests/test-writer-ui.c extras/wordprocessor/writer_document.c \
		extras/kui/kui_style.c extras/kui/kui_style_win95.c \
		extras/kui/kui_style_aqua.c -o /tmp/os3-test-writer-ui
	/tmp/os3-test-writer-ui /tmp/os3-writer-host

test-kui-scaled-text: gui-assets
	$(CC) -std=gnu11 -Wall -Wextra -Werror -I $(KERNEL_ROOT) \
		tests/test-kui-scaled-text.c extras/kui/kui.c \
		extras/kui/kui_style.c extras/kui/kui_style_win95.c \
		extras/kui/kui_style_aqua.c -o /tmp/os3-test-kui-scaled-text
	/tmp/os3-test-kui-scaled-text

test-paint-brush:
	$(CC) -std=c11 -Wall -Wextra -Werror -I $(KERNEL_ROOT) \
		tests/test-paint-brush.c extras/paint/paint_brush.c \
		-o /tmp/os3-test-paint-brush
	/tmp/os3-test-paint-brush

test-tetris:
	$(CC) -std=c11 -Wall -Wextra -Werror -I $(KERNEL_ROOT) \
		tests/test-tetris.c extras/tetris/tetris_game.c \
		-o /tmp/os3-test-tetris
	/tmp/os3-test-tetris

test-fps:
	$(CC) -std=c11 -Wall -Wextra -Werror -I $(KERNEL_ROOT) \
		tests/test-fps.c extras/fps/fps_game.c -lm \
		-o /tmp/os3-test-fps
	/tmp/os3-test-fps

test-ksheet:
	$(CC) -std=c11 -Wall -Wextra -Werror -I $(KERNEL_ROOT) \
		tests/test-ksheet.c extras/ksheet/ksheet_model.c \
		-o /tmp/os3-test-ksheet
	/tmp/os3-test-ksheet

test-ksheet-ui:
	$(CC) -std=gnu11 -Wall -Wextra -Werror -I $(KERNEL_ROOT) \
		-DKYLEOS_HOST extras/ksheet/ksheet.c extras/ksheet/ksheet_model.c \
		extras/kui/kui.c extras/kui/kui_client.c \
		extras/kui/kui_file_dialog.c extras/kui/kui_menu.c \
		extras/kui/kui_text_selection.c extras/kui/kui_style.c \
		extras/kui/kui_style_win95.c extras/kui/kui_style_aqua.c \
		-o /tmp/os3-ksheet-host
	$(CC) -std=gnu11 -Wall -Wextra -Werror -I $(KERNEL_ROOT) \
		tests/test-ksheet-ui.c extras/kui/kui_style.c \
		extras/kui/kui_style_win95.c extras/kui/kui_style_aqua.c \
		-o /tmp/os3-test-ksheet-ui
	/tmp/os3-test-ksheet-ui /tmp/os3-ksheet-host

test-tetris-ui: gui-assets
	$(CC) -std=gnu11 -Wall -Wextra -Werror -I $(KERNEL_ROOT) \
		extras/tetris/tetris.c extras/tetris/tetris_game.c \
		extras/kui/kui.c extras/kui/kui_client.c extras/kui/kui_menu.c \
		extras/kui/kui_style.c extras/kui/kui_style_win95.c \
		extras/kui/kui_style_aqua.c -o /tmp/os3-tetris-host
	$(CC) -std=gnu11 -Wall -Wextra -Werror -I $(KERNEL_ROOT) \
		tests/test-tetris-ui.c extras/kui/kui.c extras/kui/kui_style.c \
		extras/kui/kui_style_win95.c extras/kui/kui_style_aqua.c \
		-o /tmp/os3-test-tetris-ui
	/tmp/os3-test-tetris-ui /tmp/os3-tetris-host

test-fps-ui:
	$(CC) -std=gnu11 -Wall -Wextra -Werror -I $(KERNEL_ROOT) \
		extras/fps/fps.c extras/fps/fps_game.c \
		extras/kui/kui.c extras/kui/kui_client.c \
		extras/kui/kui_style.c extras/kui/kui_style_win95.c \
		extras/kui/kui_style_aqua.c -lm -o /tmp/os3-fps-host
	$(CC) -std=gnu11 -Wall -Wextra -Werror -I $(KERNEL_ROOT) \
		tests/test-fps-ui.c extras/kui/kui.c extras/kui/kui_style.c \
		extras/kui/kui_style_win95.c extras/kui/kui_style_aqua.c \
		-o /tmp/os3-test-fps-ui
	/tmp/os3-test-fps-ui /tmp/os3-fps-host

test-doom-aqua:
	$(CC) -std=gnu11 -Wall -Wextra -Werror -I $(KERNEL_ROOT) \
		tests/test-doom-aqua.c extras/doom-aqua/doom_aqua_ui.c \
		extras/kui/kui.c extras/kui/kui_style.c \
		extras/kui/kui_style_win95.c extras/kui/kui_style_aqua.c \
		-o /tmp/os3-test-doom-aqua
	/tmp/os3-test-doom-aqua
	$(CC) -std=gnu11 -Wall -Wextra -Werror -I $(KERNEL_ROOT) \
		-I extras/doom/doomgeneric \
		-DKYLEOS_HOST \
		-DDOOMGENERIC_RESX=320 -DDOOMGENERIC_RESY=200 \
		extras/doom-aqua/doomgeneric_aqua.c \
		extras/doom-aqua/doom_aqua_ui.c tests/doom-aqua-stub.c \
		extras/kui/kui.c extras/kui/kui_client.c extras/kui/kui_style.c \
		extras/kui/kui_style_win95.c extras/kui/kui_style_aqua.c \
		-o /tmp/os3-doom-aqua-host
	$(CC) -std=gnu11 -Wall -Wextra -Werror -I $(KERNEL_ROOT) \
		tests/test-doom-aqua-protocol.c extras/kui/kui.c \
		extras/kui/kui_style.c extras/kui/kui_style_win95.c \
		extras/kui/kui_style_aqua.c -o /tmp/os3-test-doom-aqua-protocol
	/tmp/os3-test-doom-aqua-protocol /tmp/os3-doom-aqua-host

test-quake-aqua:
	$(CC) -std=gnu11 -Wall -Wextra -Werror -I $(KERNEL_ROOT) \
		tests/test-quake-aqua.c extras/kui/kui.c \
		extras/kui/kui_style.c extras/kui/kui_style_win95.c \
		extras/kui/kui_style_aqua.c -o /tmp/os3-test-quake-aqua
	/tmp/os3-test-quake-aqua

test-kui-styles:
	$(CC) -std=c11 -Wall -Wextra -Werror -I $(KERNEL_ROOT) \
		tests/test-kui-styles.c extras/kui/kui_style.c \
		extras/kui/kui_style_win95.c extras/kui/kui_style_aqua.c \
		-o /tmp/os3-test-kui-styles
	/tmp/os3-test-kui-styles

test-kui-text-selection:
	$(CC) -std=c11 -Wall -Wextra -Werror -I $(KERNEL_ROOT) \
		tests/test-kui-text-selection.c extras/kui/kui_text_selection.c \
		-o /tmp/os3-test-kui-text-selection
	/tmp/os3-test-kui-text-selection

test-kui-context-menu:
	$(CC) -std=gnu11 -Wall -Wextra -Werror -I $(KERNEL_ROOT) \
		tests/test-kui-context-menu.c extras/kui/kui_menu.c \
		extras/kui/kui.c extras/kui/kui_style.c \
		extras/kui/kui_style_win95.c extras/kui/kui_style_aqua.c \
		-o /tmp/os3-test-kui-context-menu
	/tmp/os3-test-kui-context-menu

test-editor-clipboard:
	$(CC) -std=gnu11 -Wall -Wextra -Werror -I $(KERNEL_ROOT) \
		-DKYLEOS_HOST extras/editor/editor.c extras/kui/kui.c extras/kui/kui_client.c \
		extras/kui/kui_file_dialog.c extras/kui/kui_dialog.c \
		extras/kui/kui_menu.c extras/kui/kui_text_selection.c \
		extras/kui/kui_style.c extras/kui/kui_style_win95.c \
		extras/kui/kui_style_aqua.c -o /tmp/os3-editor-host
	$(CC) -std=gnu11 -Wall -Wextra -Werror -I $(KERNEL_ROOT) \
		tests/test-editor-clipboard.c extras/kui/kui_style.c \
		extras/kui/kui_style_win95.c extras/kui/kui_style_aqua.c \
		-o /tmp/os3-test-editor-clipboard
	/tmp/os3-test-editor-clipboard /tmp/os3-editor-host

test-terminal-vt100:
	$(CC) -std=c11 -Wall -Wextra -Werror -I $(KERNEL_ROOT) \
		tests/test-terminal-vt100.c extras/terminal/vt100.c \
		-o /tmp/os3-test-terminal-vt100
	/tmp/os3-test-terminal-vt100

test-terminal-async:
	$(CC) -std=gnu11 -Wall -Wextra -Werror -I $(KERNEL_ROOT) \
		-DKYLEOS_HOST -DTERMINAL_SHELL_PATH='"/bin/sh"' \
		extras/terminal/terminal.c extras/terminal/vt100.c \
		extras/kui/kui.c extras/kui/kui_client.c \
		extras/kui/kui_style.c extras/kui/kui_style_win95.c \
		extras/kui/kui_style_aqua.c -o /tmp/os3-terminal-host
	$(CC) -std=gnu11 -Wall -Wextra -Werror -I $(KERNEL_ROOT) \
		tests/test-terminal-async.c extras/kui/kui_style.c \
		extras/kui/kui_style_win95.c extras/kui/kui_style_aqua.c \
		-o /tmp/os3-test-terminal-async
	/tmp/os3-test-terminal-async /tmp/os3-terminal-host

test-help-ui:
	$(CC) -std=gnu11 -Wall -Wextra -Werror -I $(KERNEL_ROOT) \
		extras/help/help.c extras/kui/kui.c extras/kui/kui_client.c \
		extras/kui/kui_style.c extras/kui/kui_style_win95.c \
		extras/kui/kui_style_aqua.c -o /tmp/os3-help-host
	$(CC) -std=gnu11 -Wall -Wextra -Werror -I $(KERNEL_ROOT) \
		tests/test-help-ui.c extras/kui/kui_style.c \
		extras/kui/kui_style_win95.c extras/kui/kui_style_aqua.c \
		-o /tmp/os3-test-help-ui
	/tmp/os3-test-help-ui /tmp/os3-help-host

test-kui-shells:
	$(CC) -std=gnu11 -Wall -Wextra -Werror -I $(KERNEL_ROOT) \
		extras/kuid/win95_shell.c extras/kui/kui.c extras/kui/kui_style.c \
		extras/kui/kui_style_win95.c extras/kui/kui_style_aqua.c \
		-o /tmp/os3-win95-shell-host
	$(CC) -std=gnu11 -Wall -Wextra -Werror -I $(KERNEL_ROOT) \
		extras/kuid/aqua_shell.c extras/kui/kui.c extras/kui/kui_style.c \
		extras/kui/kui_style_win95.c extras/kui/kui_style_aqua.c \
		-o /tmp/os3-aqua-shell-host
	$(CC) -std=gnu11 -Wall -Wextra -Werror -I $(KERNEL_ROOT) \
		tests/test-kui-shells.c extras/kui/kui.c extras/kui/kui_style.c \
		extras/kui/kui_style_win95.c extras/kui/kui_style_aqua.c \
		-o /tmp/os3-test-kui-shells
	/tmp/os3-test-kui-shells /tmp/os3-win95-shell-host /tmp/os3-aqua-shell-host

kmsgtest: extras/kmsgtest/kmsgtest.c
	mkdir -p $(USERLAND_STAGE)
	$(CC) $(CFLAGS) -static -I $(SYSROOT)/include $< $(SYSROOT)/lib/libc.a $(SYSROOT)/lib/libm.a -o $(USERLAND_STAGE)/kmsgtest

fstest: extras/fstest/fstest.c
	mkdir -p $(USERLAND_STAGE)
	$(CC) $(CFLAGS) -static -I $(SYSROOT)/include $< $(SYSROOT)/lib/libc.a $(SYSROOT)/lib/libm.a -o $(USERLAND_STAGE)/fstest

gpfault: extras/gpfault/gpfault.c
	mkdir -p $(USERLAND_STAGE)
	$(CC) $(CFLAGS) -static -I $(SYSROOT)/include $< $(SYSROOT)/lib/libc.a $(SYSROOT)/lib/libm.a -o $(USERLAND_STAGE)/gpfault

waittest: extras/waittest/waittest.c
	mkdir -p $(USERLAND_STAGE)
	$(CC) $(CFLAGS) -static -I $(SYSROOT)/include $< $(SYSROOT)/lib/libc.a $(SYSROOT)/lib/libm.a -o $(USERLAND_STAGE)/waittest

pipetest: extras/pipetest/pipetest.c
	mkdir -p $(USERLAND_STAGE)
	$(CC) $(CFLAGS) -static -I $(SYSROOT)/include $< $(SYSROOT)/lib/libc.a $(SYSROOT)/lib/libm.a -o $(USERLAND_STAGE)/pipetest

polltest: extras/polltest/polltest.c
	mkdir -p $(USERLAND_STAGE)
	$(CC) $(CFLAGS) -static -I $(SYSROOT)/include $< $(SYSROOT)/lib/libc.a $(SYSROOT)/lib/libm.a -o $(USERLAND_STAGE)/polltest

signaltest: extras/signaltest/signaltest.c
	mkdir -p $(USERLAND_STAGE)
	$(CC) $(CFLAGS) -static -I $(SYSROOT)/include $< $(SYSROOT)/lib/libc.a $(SYSROOT)/lib/libm.a -o $(USERLAND_STAGE)/signaltest

ttytest: extras/ttytest/ttytest.c
	mkdir -p $(USERLAND_STAGE)
	$(CC) $(CFLAGS) -static -I $(SYSROOT)/include $< $(SYSROOT)/lib/libc.a -o $(USERLAND_STAGE)/ttytest

ptytest: extras/ptytest/ptytest.c
	mkdir -p $(USERLAND_STAGE)
	$(CC) $(CFLAGS) -static -I $(SYSROOT)/include $< $(SYSROOT)/lib/libc.a -o $(USERLAND_STAGE)/ptytest

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

doom-aqua: $(KUI_LIBRARY)
	SYSROOT=$(SYSROOT) KUI_LIBRARY=$(KUI_LIBRARY) scripts/build-doom-aqua.sh

doom-aqua-image:
	$(MAKE) doom-aqua
	$(MAKE) image-reset

quake:
	SYSROOT=$(SYSROOT) scripts/build-quake.sh

quake-image:
	$(MAKE) quake
	$(MAKE) image-reset

quake-aqua: $(KUI_LIBRARY)
	SYSROOT=$(SYSROOT) KUI_LIBRARY=$(KUI_LIBRARY) scripts/build-quake-aqua.sh

quake-aqua-image:
	$(MAKE) quake-aqua
	$(MAKE) image-reset

# Windowed extras link libkui statically but are not part of the normal
# userland target. Never package an existing client built against an older
# KUI protocol after userland has refreshed the library.
refresh-kui-extras:
	@if [ -x $(DOOM_AQUA_BINARY) ] && [ $(KUI_LIBRARY) -nt $(DOOM_AQUA_BINARY) ]; then \
		echo "Rebuilding stale Doom Aqua client"; \
		$(MAKE) doom-aqua; \
	fi
	@if [ -x $(QUAKE_AQUA_BINARY) ] && [ $(KUI_LIBRARY) -nt $(QUAKE_AQUA_BINARY) ]; then \
		echo "Rebuilding stale Quake Aqua client"; \
		$(MAKE) quake-aqua; \
	fi

duke3d:
	SYSROOT=$(SYSROOT) scripts/build-duke3d.sh

duke3d-image:
	$(MAKE) duke3d
	$(MAKE) image-reset

busybox:
	SYSROOT=$(SYSROOT) BUSYBOX_BINARY=$(BUSYBOX_BINARY) scripts/build-busybox.sh

busybox-image: busybox
	$(MAKE) image-reset

lua: $(LUA_BINARY)

$(LUA_BINARY): $(LUA_INPUTS)
	$(MAKE) -C $(LUA_SOURCE) clean
	$(MAKE) -C $(LUA_SOURCE) lua SYSROOT=$(SYSROOT)
	mkdir -p $(dir $(LUA_BINARY))
	cp $(LUA_SOURCE)/lua $(LUA_BINARY)

image: userland lua busybox
	$(MAKE) refresh-kui-extras
	IMAGE_PATH=$(IMAGE) IMAGE_MOUNT=$(IMAGE_MOUNT) USERLAND_STAGE=$(USERLAND_STAGE) CORE_SRC=$(CORE_SRC) PROGS_SRC=$(PROGS_SRC) BUSYBOX_BINARY=$(BUSYBOX_BINARY) DOOM_BINARY=$(DOOM_BINARY) DOOM_AQUA_BINARY=$(DOOM_AQUA_BINARY) DOOM_WAD=$(DOOM_WAD) QUAKE_BINARY=$(QUAKE_BINARY) QUAKE_AQUA_BINARY=$(QUAKE_AQUA_BINARY) QUAKE_DATA_DIR=$(QUAKE_DATA_DIR) DUKE3D_BINARY=$(DUKE3D_BINARY) DUKE3D_GRP=$(DUKE3D_GRP) LUA_BINARY=$(LUA_BINARY) BIBLE_TEXT=$(BIBLE_TEXT) TCC_RUNTIME=$(KERNEL_ROOT)/build/extras/tinycc/root scripts/image-create.sh

image-reset: userland lua busybox
	$(MAKE) refresh-kui-extras
	IMAGE_PATH=$(IMAGE) IMAGE_MOUNT=$(IMAGE_MOUNT) USERLAND_STAGE=$(USERLAND_STAGE) CORE_SRC=$(CORE_SRC) PROGS_SRC=$(PROGS_SRC) BUSYBOX_BINARY=$(BUSYBOX_BINARY) DOOM_BINARY=$(DOOM_BINARY) DOOM_AQUA_BINARY=$(DOOM_AQUA_BINARY) DOOM_WAD=$(DOOM_WAD) QUAKE_BINARY=$(QUAKE_BINARY) QUAKE_AQUA_BINARY=$(QUAKE_AQUA_BINARY) QUAKE_DATA_DIR=$(QUAKE_DATA_DIR) DUKE3D_BINARY=$(DUKE3D_BINARY) DUKE3D_GRP=$(DUKE3D_GRP) LUA_BINARY=$(LUA_BINARY) BIBLE_TEXT=$(BIBLE_TEXT) TCC_RUNTIME=$(KERNEL_ROOT)/build/extras/tinycc/root scripts/image-create.sh --reset

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
	$(QEMU) $(QEMU_OPTS) -m 64M -vga std -kernel kernel32.bin -hda $(IMAGE) -serial stdio

test-nox: kernel32.bin image
	$(QEMU) $(QEMU_OPTS) -m 4G -kernel kernel32.bin -hda $(IMAGE) -display none -serial stdio
test-log: kernel32.bin image
	$(QEMU) $(QEMU_OPTS) -m 8G -kernel kernel32.bin -d int,cpu_reset -hda $(IMAGE) -serial stdio 2>log
kvm-test: kernel32.bin image
	$(QEMU) $(QEMU_OPTS) -m 8G -kernel kernel32.bin -d int,cpu_reset -hda $(IMAGE) -serial stdio -enable-kvm 2>log

test-c: kernel32.bin image
	$(QEMU) $(QEMU_OPTS) -m 8G -kernel kernel32.bin -d int,cpu_reset -hda $(IMAGE) -display none -serial stdio 2>log
debug: kernel32.bin
	$(QEMU) $(QEMU_OPTS) -m 8G -kernel kernel32.bin -hda $(IMAGE) -serial stdio -s -S
debug-nox: kernel32.bin
	$(QEMU) $(QEMU_OPTS) -m 8G -display none -kernel kernel32.bin -hda $(IMAGE) -serial stdio -s -S

gdb: kernel.bin
	gdb -ex "target remote localhost:1234" kernel.bin


iso: kernel.bin $(GRUB_CFG)
	rm -rf $(GRUB_ISO_DIR)
	mkdir -p $(GRUB_ISO_DIR)/boot/grub
	cp kernel.bin $(GRUB_ISO_DIR)/boot/kernel.bin
	cp $(GRUB_CFG) $(GRUB_ISO_DIR)/boot/grub/grub.cfg
	grub-mkrescue -o bootable.iso $(GRUB_ISO_DIR)

iso-test: iso
	$(QEMU) $(QEMU_OPTS) \
		-m 60M \
		-cdrom bootable.iso \
		-serial stdio

test-unit: $(HOST_TEST_TARGETS)

test-e2e: kernel32.bin
	$(MAKE) image-reset IMAGE=$(TEST_IMAGE) IMAGE_MOUNT=$(TEST_IMAGE_MOUNT)
	$(MAKE) test-e2e-run

test-e2e-run: kernel32.bin
	QEMU=$(QEMU) QEMU_OPTS='$(QEMU_OPTS)' TEST_KERNEL=$(KERNEL_ROOT)/kernel32.bin TEST_IMAGE=$(TEST_IMAGE) TEST_TIMEOUT=$(TEST_TIMEOUT) bash tests/e2e/all-tests.sh

test-suite: test-unit
	$(MAKE) test-e2e

test-busybox: kernel32.bin busybox
	$(MAKE) image-reset IMAGE=$(TEST_IMAGE) IMAGE_MOUNT=$(TEST_IMAGE_MOUNT)
	$(MAKE) test-busybox-run

test-busybox-run: kernel32.bin
	QEMU=$(QEMU) QEMU_OPTS='$(QEMU_OPTS)' TEST_KERNEL=$(KERNEL_ROOT)/kernel32.bin TEST_IMAGE=$(TEST_IMAGE) bash tests/e2e/busybox-test.sh
