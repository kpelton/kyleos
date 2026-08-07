#!/usr/bin/env bash
set -euo pipefail

root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
doom_repo="$root/extras/doom"
patch="$root/patches/doom/0001-kyleos-port.patch"
backend="$root/extras/doom-kyleos/doomgeneric_kyleos.c"
output="$root/build/extras/doom/doom"
work_dir="$root/build/extras/doom/src"

[[ -e $doom_repo/.git ]] || { echo "Doom submodule is not initialized" >&2; exit 1; }
rm -rf -- "$work_dir"
mkdir -p "$work_dir"
git -C "$doom_repo" archive HEAD | tar -x -C "$work_dir"
patch -d "$work_dir" -p1 < "$patch"
doom_dir="$work_dir/doomgeneric"
cp "$backend" "$doom_dir/doomgeneric_kyleos.c"
make -C "$doom_dir" NEWLIB_INSTALL="${SYSROOT:-/tmp/z}"
mkdir -p "$(dirname -- "$output")"
cp "$doom_dir/doomgeneric" "$output"
