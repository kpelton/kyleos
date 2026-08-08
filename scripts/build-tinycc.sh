#!/usr/bin/env bash
set -euo pipefail

root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
source_dir=${TCC_SOURCE:-"$root/extras/tinycc"}
patched_source=${TCC_PATCHED_SOURCE:-"$root/build/extras/tinycc/src"}
patched_port=${TCC_PATCHED_PORT:-"$root/build/extras/tinycc/tinycc-kyleos"}
sysroot=${SYSROOT:-/tmp/z}
output=${TCC_BINARY:-"$root/build/userland/cc"}
runtime=${TCC_RUNTIME:-"$root/build/extras/tinycc/root"}
host_cc=${HOST_CC:-gcc}

[[ -f $source_dir/tcc.c ]] || {
    echo "missing TinyCC source: $source_dir" >&2
    exit 1
}
[[ -f $sysroot/lib/libc.a ]] || {
    echo "missing KyleOS newlib: $sysroot/lib/libc.a" >&2
    exit 1
}

rm -rf -- "$patched_source" "$patched_port"
mkdir -p "$patched_source" "$patched_port"
cp -R "$source_dir"/. "$patched_source/"
cp -R "$root/extras/tinycc-kyleos"/. "$patched_port/"
rm -rf -- "$patched_source/.git"
patch -d "$patched_source" -p1 < "$patched_port/tinycc-kyleos.patch"

if [[ ! -f $patched_source/tccdefs_.h ]]; then
    "$host_cc" -DC2STR "$patched_source/conftest.c" \
        -o "$patched_source/c2str.exe"
    "$patched_source/c2str.exe" "$patched_source/include/tccdefs.h" \
        "$patched_source/tccdefs_.h"
fi

mkdir -p "$(dirname -- "$output")" "$runtime/usr/lib/tcc/include" \
         "$runtime/usr/include" "$runtime/usr/lib"

"$host_cc" -m64 -O1 -g -mstackrealign -nostdlib -fno-builtin -nostartfiles \
    -nodefaultlibs -ffreestanding -mcmodel=large -mno-red-zone \
    -fno-stack-protector -static -I "$sysroot/include" -I "$patched_source" \
    -DONE_SOURCE -DCONFIG_KYLEOS \
    "$patched_source/tcc.c" "$patched_port/compat.c" \
    "$sysroot/lib/libc.a" "$sysroot/lib/libm.a" -o "$output"
if [[ ${TCC_STRIP:-1} -eq 1 ]]; then
    strip "$output"
fi

cp -R "$patched_source/include"/. "$runtime/usr/lib/tcc/include/"
cp -R "$sysroot/include"/. "$runtime/usr/include/"
cp "$sysroot/lib/libc.a" "$runtime/usr/lib/libc.a"
cp "$sysroot/lib/libm.a" "$runtime/usr/lib/libm.a"
"$host_cc" -m64 -O1 -c -ffreestanding -fno-stack-protector \
    -I "$sysroot/include" "$root/extras/tinycc-kyleos/crt0.c" \
    -o "$runtime/usr/lib/crt0.o"
"$host_cc" -m64 -O1 -c -ffreestanding -fno-builtin -fno-stack-protector \
    "$patched_source/lib/libtcc1.c" -o "$runtime/usr/lib/tcc/libtcc1.o"
"$host_cc" -m64 -O1 -c -ffreestanding -fno-builtin -fno-stack-protector \
    "$patched_source/lib/va_list.c" -o "$runtime/usr/lib/tcc/va_list.o"
ar rcs "$runtime/usr/lib/tcc/libtcc1.a" \
    "$runtime/usr/lib/tcc/libtcc1.o" "$runtime/usr/lib/tcc/va_list.o"
