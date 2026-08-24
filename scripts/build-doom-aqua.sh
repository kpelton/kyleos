#!/usr/bin/env bash
set -euo pipefail

root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
doom_repo="$root/extras/doom"
port_dir="$root/extras/doom-aqua"
patch_file="$root/patches/doom/0001-kyleos-port.patch"
output="$root/build/extras/doom-aqua/doom-aqua"
work_dir="$root/build/extras/doom-aqua/src"

[[ -e $doom_repo/.git ]] || {
    echo "Doom submodule is not initialized" >&2
    exit 1
}

case $work_dir in
    "$root/build/extras/doom-aqua/src") ;;
    *) echo "refusing unsafe Doom Aqua work directory: $work_dir" >&2; exit 1 ;;
esac
rm -rf -- "$work_dir"
mkdir -p "$work_dir"
git -C "$doom_repo" archive HEAD | tar -x -C "$work_dir"
patch -d "$work_dir" -p1 < "$patch_file"
doom_dir="$work_dir/doomgeneric"
cp "$port_dir/Makefile" "$port_dir"/*.c "$doom_dir/"
make -C "$doom_dir" ROOT="$root" \
    NEWLIB_INSTALL="${SYSROOT:-$root/build/sysroot}" \
    KUI_LIBRARY="${KUI_LIBRARY:-$root/build/userland/libkui.a}"
mkdir -p "$(dirname -- "$output")"
cp "$doom_dir/doom-aqua" "$output"
