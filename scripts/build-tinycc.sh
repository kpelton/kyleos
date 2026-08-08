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

# The compiler itself is one translation unit (tcc.c), but the target-side
# runtime objects are independent and can use the parent's make -j setting.
tcc_jobs=${TCC_JOBS:-}
if [[ -z $tcc_jobs ]]; then
    for make_flag in ${MAKEFLAGS:-}; do
        case $make_flag in
            --jobs=*[0-9]) tcc_jobs=${make_flag#*=} ;;
            -j[0-9]*) tcc_jobs=${make_flag#-j} ;;
            -j|--jobs) tcc_jobs=$(getconf _NPROCESSORS_ONLN 2>/dev/null || echo 1) ;;
        esac
    done
fi
[[ ${tcc_jobs:-} =~ ^[1-9][0-9]*$ ]] || tcc_jobs=1

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
build_crt0()
{
    "$host_cc" -m64 -O1 -c -ffreestanding -fno-stack-protector \
        -I "$sysroot/include" "$root/extras/tinycc-kyleos/crt0.c" \
        -o "$runtime/usr/lib/crt0.o"
}

build_libtcc1()
{
    "$host_cc" -m64 -O1 -c -ffreestanding -fno-builtin -fno-stack-protector \
        "$patched_source/lib/libtcc1.c" -o "$runtime/usr/lib/tcc/libtcc1.o"
}

build_va_list()
{
    "$host_cc" -m64 -O1 -c -ffreestanding -fno-builtin -fno-stack-protector \
        "$patched_source/lib/va_list.c" -o "$runtime/usr/lib/tcc/va_list.o"
}

if (( tcc_jobs > 1 )); then
    build_crt0 & pid_crt0=$!
    build_libtcc1 & pid_libtcc1=$!
    build_va_list & pid_va_list=$!
    wait "$pid_crt0" "$pid_libtcc1" "$pid_va_list"
else
    build_crt0
    build_libtcc1
    build_va_list
fi
ar rcs "$runtime/usr/lib/tcc/libtcc1.a" \
    "$runtime/usr/lib/tcc/libtcc1.o" "$runtime/usr/lib/tcc/va_list.o"
