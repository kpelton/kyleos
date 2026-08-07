#!/usr/bin/env bash
set -euo pipefail

root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
doom_repo="$root/extras/doom"
doom_dir="$doom_repo/doomgeneric"
patch="$root/patches/doom/0001-kyleos-port.patch"
backend="$root/extras/doom-kyleos/doomgeneric_kyleos.c"
output="$root/build/extras/doom/doom"

[[ -e $doom_repo/.git ]] || { echo "Doom submodule is not initialized" >&2; exit 1; }
if ! git -C "$doom_repo" apply --reverse --check "$patch" >/dev/null 2>&1; then
    git -C "$doom_repo" apply "$patch"
fi
cp "$backend" "$doom_dir/doomgeneric_kyleos.c"
make -C "$doom_dir" NEWLIB_INSTALL="${SYSROOT:-/tmp/z}"
mkdir -p "$(dirname -- "$output")"
cp "$doom_dir/doomgeneric" "$output"
