#!/usr/bin/env bash
set -euo pipefail

root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
duke_repo="$root/extras/duke3d"
port_dir="$root/extras/duke3d-kyleos"
patch_file="$root/patches/duke3d/0001-kyleos-port.patch"
output="$root/build/extras/duke3d/duke3d"
work_dir="$root/build/extras/duke3d/src"

[[ -e $duke_repo/.git ]] || {
    echo "Duke3D submodule is not initialized" >&2
    exit 1
}

[[ -f ${SYSROOT:-$root/build/sysroot}/lib/libc.a ]] || {
    echo "missing Newlib sysroot: run 'make toolchain' first" >&2
    exit 1
}

case $work_dir in
    "$root/build/extras/duke3d/src") ;;
    *) echo "refusing unsafe Duke3D work directory: $work_dir" >&2; exit 1 ;;
esac
[[ ! -L $work_dir ]] || {
    echo "refusing symlinked Duke3D work directory: $work_dir" >&2
    exit 1
}

rm -rf -- "$work_dir"
mkdir -p "$work_dir"
git -C "$duke_repo" archive HEAD | tar -x -C "$work_dir"
patch -d "$work_dir" -p1 < "$patch_file"
cp -R "$port_dir" "$work_dir/kyleos"
cp "$port_dir/Makefile" "$work_dir/Makefile.kyleos"

make -C "$work_dir" -f Makefile.kyleos \
    CC="${CC:-gcc}" NEWLIB_INSTALL="${SYSROOT:-$root/build/sysroot}"
mkdir -p "$(dirname -- "$output")"
cp "$work_dir/duke3d" "$output"
