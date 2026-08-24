#!/usr/bin/env bash
set -euo pipefail

root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
quake_repo="$root/extras/quake"
port_dir="$root/extras/quake-aqua"
output="$root/build/extras/quake-aqua/quake-aqua"
work_dir="$root/build/extras/quake-aqua/src"

[[ -e $quake_repo/.git ]] || {
    echo "Quake submodule is not initialized" >&2
    exit 1
}

case $work_dir in
    "$root/build/extras/quake-aqua/src") ;;
    *) echo "refusing unsafe Quake Aqua work directory: $work_dir" >&2; exit 1 ;;
esac
rm -rf -- "$work_dir"
mkdir -p "$work_dir"
git -C "$quake_repo" archive HEAD | tar -x -C "$work_dir"
for patch_file in "$root"/patches/quake/*.patch; do
    patch -d "$work_dir" -p1 < "$patch_file"
done
cp "$port_dir"/Makefile "$port_dir"/*.c "$port_dir"/*.h \
    "$work_dir/WinQuake/"
make -C "$work_dir/WinQuake" ROOT="$root" \
    NEWLIB_INSTALL="${SYSROOT:-$root/build/sysroot}" \
    KUI_LIBRARY="${KUI_LIBRARY:-$root/build/userland/libkui.a}"
mkdir -p "$(dirname -- "$output")"
cp "$work_dir/WinQuake/quake-aqua" "$output"
