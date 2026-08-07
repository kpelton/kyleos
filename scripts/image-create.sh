#!/usr/bin/env bash
set -euo pipefail

root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
image=${IMAGE_PATH:-"$root/build/image/test-hd.img"}
mount_dir=${IMAGE_MOUNT:-"$root/build/image/mnt"}
stage_dir=${USERLAND_STAGE:-"$root/build/userland"}
seed_dir="$root/image/rootfs"
doom_binary=${DOOM_BINARY:-"$root/build/extras/doom/doom"}
doom_wad=${DOOM_WAD:-"$root/assets/doom.wad"}
ready_file="$image.ready"
fdisk_bin=${FDISK_BIN:-/sbin/fdisk}
kpartx_bin=${KPARTX_BIN:-/sbin/kpartx}
mkfs_fat_bin=${MKFS_FAT_BIN:-/sbin/mkfs.fat}
reset=0

if [[ ${1:-} == "--reset" ]]; then
    reset=1
elif [[ $# -ne 0 ]]; then
    echo "usage: $0 [--reset]" >&2
    exit 2
fi

if mountpoint -q "$mount_dir"; then
    echo "image mount point is already mounted: $mount_dir" >&2
    exit 1
fi
if [[ -f $image && -f $ready_file && $reset -eq 0 ]]; then
    exit 0
fi

for tool in "$fdisk_bin" "$kpartx_bin" "$mkfs_fat_bin"; do
    [[ -x $tool ]] || {
        echo "missing required tool: $tool" >&2
        exit 1
    }
done
if [[ ! -d $stage_dir ]]; then
    echo "missing staged userland: run 'make userland' first" >&2
    exit 1
fi

mkdir -p "$(dirname -- "$image")" "$mount_dir"
rm -f -- "$image"
rm -f -- "$ready_file"
dd if=/dev/zero of="$image" bs=1M count="${IMAGE_SIZE_MB:-256}" status=none
"$fdisk_bin" "$image" <<'EOF'
o
n
p
1
2048

t
c
w
EOF

mapper=""
mounted=0
completed=0
cleanup() {
    if [[ $mounted -eq 1 ]]; then
        sudo umount "$mount_dir" || true
    fi
    if [[ -n $mapper ]]; then
        sudo "$kpartx_bin" -d "$image" || true
    fi
    if [[ $completed -eq 0 ]]; then
        rm -f -- "$image" "$ready_file"
    fi
}
trap cleanup EXIT

map_output=$(sudo "$kpartx_bin" -av "$image")
mapper=$(awk '/add map/ { print $3; exit }' <<<"$map_output")
if [[ -z $mapper ]]; then
    echo "kpartx did not create a partition mapper" >&2
    exit 1
fi
device="/dev/mapper/$mapper"
sudo "$mkfs_fat_bin" -F 32 "$device" >/dev/null
sudo mount -o "uid=$(id -u),gid=$(id -g),umask=022" "$device" "$mount_dir"
mounted=1

cp -a "$seed_dir"/. "$mount_dir"/
mkdir -p "$mount_dir/2"
while IFS= read -r program; do
    [[ -z $program || $program == \#* ]] && continue
    [[ -f $stage_dir/$program ]] || {
        echo "staged program missing: $program" >&2
        exit 1
    }
    cp "$stage_dir/$program" "$mount_dir/$program"
done < "$root/image/manifest.txt"
if [[ -f $doom_binary ]]; then
    mkdir -p "$mount_dir/usr/bin" "$mount_dir/usr/share/doom"
    cp "$doom_binary" "$mount_dir/usr/bin/doom"
    if [[ -f $doom_wad ]]; then
        cp "$doom_wad" "$mount_dir/usr/share/doom/doom.wad"
    else
        echo "warning: Doom binary installed without a WAD; set DOOM_WAD=/path/to/doom.wad" >&2
    fi
fi
sync
touch "$ready_file"
completed=1
