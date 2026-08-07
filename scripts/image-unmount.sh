#!/usr/bin/env bash
set -euo pipefail

root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
image=${IMAGE_PATH:-"$root/build/image/test-hd.img"}
mount_dir=${IMAGE_MOUNT:-"$root/build/image/mnt"}
kpartx_bin=${KPARTX_BIN:-/sbin/kpartx}

mountpoint -q "$mount_dir" || { echo "not mounted: $mount_dir" >&2; exit 1; }
sudo umount "$mount_dir"
sudo "$kpartx_bin" -d "$image"
rm -f "$root/build/image/mapper"
