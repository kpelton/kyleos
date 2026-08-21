#!/bin/sh
# Minimal kernel builder for running inside KyleOS.
#
# This deliberately avoids make, find, and host-specific image tools.  Copy
# the repository to /src/kyleos, install this file as /bin/kernel-build, and
# run it from that directory (or pass the source directory as argv[1]).

set -e

ROOT=${1:-/src/kyleos}
OUT=${2:-$ROOT/build/selfhost}

CC=${CC:-cc}
AS=${AS:-nasm}
LD=${LD:-ld}

# Keep the default set accepted by the native TinyCC currently shipped in
# KyleOS.  A GCC-based bootstrap may add -mcmodel=large and -mno-red-zone via
# CFLAGS, but TinyCC rejects GCC's -nostartfiles/-nodefaultlibs switches.
CFLAGS=${CFLAGS:-"-m64 -O2 -Wall -Wextra -nostdlib -ffreestanding -I $ROOT -g"}

mkdir -p "$OUT"

compile() {
    src=$1
    obj=$2
    echo "CC $src"
    "$CC" $CFLAGS -c "$ROOT/$src" -o "$OUT/$obj"
}

compile asm/asm.c asm.o
compile block/ata.c ata.o
compile fs/fat.c fat.o
compile fs/ramfs.c ramfs.o
compile fs/vfs.c vfs.o
compile fs/pipe.c pipe.o
compile init/kernel.c kernel.o
compile init/tables.c tables.o
compile init/dshell.c dshell.o
compile init/syscall.c syscall.o
compile irq/irq.c irq.o
compile locks/spinlock.c spinlock.o
compile locks/mutex.c mutex.o
compile mm/paging.c paging.o
compile mm/mm.c mm.o
compile mm/pmem.c pmem.o
compile mm/vmm.c vmm.o
compile mm/mtrr.c mtrr.o
compile output/keyboard.c keyboard.o
compile output/uart.c uart.o
compile output/vga.c vga.o
compile output/framebuffer.c framebuffer.o
compile output/output.c output.o
compile output/input.c input.o
compile output/mouse.c mouse.o
compile sched/sched.c sched.o
compile sched/ps.c ps.o
compile sched/exec.c exec.o
compile timer/timer.c timer.o
compile timer/rtc.c rtc.o
compile timer/pit.c pit.o
compile utils/llist.c llist.o

echo "AS init/loader.s"
"$AS" -f elf64 -o "$OUT/loader.o" "$ROOT/init/loader.s"
echo "AS asm/asm_calls.s"
"$AS" -f elf64 -o "$OUT/asm_calls.o" "$ROOT/asm/asm_calls.s"

echo "LD kernel.bin"
"$LD" -m elf_x86_64 -T "$ROOT/linker.ld" -o "$OUT/kernel.bin" \
    "$OUT/asm.o" "$OUT/asm_calls.o" "$OUT/ata.o" "$OUT/fat.o" \
    "$OUT/ramfs.o" "$OUT/vfs.o" "$OUT/pipe.o" "$OUT/kernel.o" \
    "$OUT/tables.o" "$OUT/dshell.o" "$OUT/loader.o" "$OUT/syscall.o" \
    "$OUT/irq.o" "$OUT/spinlock.o" "$OUT/mutex.o" "$OUT/paging.o" \
    "$OUT/mm.o" "$OUT/pmem.o" "$OUT/vmm.o" "$OUT/mtrr.o" \
    "$OUT/keyboard.o" "$OUT/uart.o" "$OUT/vga.o" "$OUT/framebuffer.o" \
    "$OUT/output.o" "$OUT/input.o" "$OUT/mouse.o" "$OUT/sched.o" \
    "$OUT/ps.o" "$OUT/exec.o" "$OUT/timer.o" "$OUT/rtc.o" \
    "$OUT/pit.o" "$OUT/llist.o"

echo "Built $OUT/kernel.bin"
