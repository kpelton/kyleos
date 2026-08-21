#!/usr/bin/env bash
set -euo pipefail

root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
quake_repo="$root/extras/quake"
port_dir="$root/extras/quake-kyleos"
output="$root/build/extras/quake/quake"
work_dir="$root/build/extras/quake/src"

[[ -e $quake_repo/.git ]] || {
    echo "Quake submodule is not initialized" >&2
    exit 1
}

rm -rf -- "$work_dir"
mkdir -p "$work_dir"
git -C "$quake_repo" archive HEAD | tar -x -C "$work_dir"
for patch_file in "$root"/patches/quake/*.patch; do
    patch -d "$work_dir" -p1 < "$patch_file"
done
cp "$port_dir"/Makefile "$port_dir"/*.c "$port_dir"/*.h \
    "$work_dir/WinQuake/"

make -C "$work_dir/WinQuake" NEWLIB_INSTALL="${SYSROOT:-$root/build/sysroot}"
mkdir -p "$(dirname -- "$output")"
cp "$work_dir/WinQuake/quake" "$output"
