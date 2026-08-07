#!/usr/bin/env bash
set -euo pipefail

root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
image=${IMAGE_PATH:-"$root/build/image/test-hd.img"}
mount_dir=${IMAGE_MOUNT:-"$root/build/image/mnt"}
kpartx_bin=${KPARTX_BIN:-/sbin/kpartx}

[[ -f $image ]] || { echo "missing image: run make image" >&2; exit 1; }
mountpoint -q "$mount_dir" && { echo "already mounted: $mount_dir" >&2; exit 1; }
mkdir -p "$mount_dir"
[[ -x $kpartx_bin ]] || { echo "missing required tool: $kpartx_bin" >&2; exit 1; }
map_output=$(sudo "$kpartx_bin" -av "$image")
mapper=$(awk '/add map/ { print $3; exit }' <<<"$map_output")
[[ -n $mapper ]] || { echo "kpartx did not create a mapper" >&2; exit 1; }
sudo mount "/dev/mapper/$mapper" "$mount_dir"
echo "$mapper" > "$root/build/image/mapper"
echo "mounted at $mount_dir"
