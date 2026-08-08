#!/usr/bin/env bash
set -euo pipefail

root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)

image=${IMAGE_PATH:-"$root/build/image/test-hd.img"}

# If ROOTFS_DIR is set, build directly into that directory instead of
# creating/mounting a disk image.
if [[ -n ${ROOTFS_DIR:-} ]]; then
    mount_dir=$ROOTFS_DIR
    directory_mode=1
else
    mount_dir=${IMAGE_MOUNT:-"$root/build/image/mnt"}
    directory_mode=0
fi

stage_dir=${USERLAND_STAGE:-"$root/build/userland"}
seed_dir="$root/image/rootfs"

doom_binary=${DOOM_BINARY:-"$root/build/extras/doom/doom"}
doom_wad=${DOOM_WAD:-"$root/assets/doom.wad"}

lua_binary=${LUA_BINARY:-"$root/build/extras/lua/lua"}

bible_text=${BIBLE_TEXT:-"$root/assets/bible.txt"}

tcc_runtime=${TCC_RUNTIME:-"$root/build/extras/tinycc/root"}
tcc_source=${TCC_SOURCE:-"$root/build/extras/tinycc/src"}
tcc_port=${TCC_PORT:-"$root/build/extras/tinycc/tinycc-kyleos"}

core_source=${CORE_SRC:-"$root/../../kyleos-userspace"}
progs_source=${PROGS_SRC:-"$root/../../newlib-progs"}

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


# ----------------------------------------------------------------------
# Validate
# ----------------------------------------------------------------------

if [[ $directory_mode -eq 0 ]]; then
    if mountpoint -q "$mount_dir"; then
        echo "image mount point is already mounted: $mount_dir" >&2
        exit 1
    fi

    # Existing completed image can be reused unless --reset was requested.
    if [[ -f $image && -f $ready_file && $reset -eq 0 ]]; then
        exit 0
    fi

    # These tools are only needed when we're actually creating an image.
    for tool in "$fdisk_bin" "$kpartx_bin" "$mkfs_fat_bin"; do
        [[ -x $tool ]] || {
            echo "missing required tool: $tool" >&2
            exit 1
        }
    done
fi

if [[ ! -d $stage_dir ]]; then
    echo "missing staged userland: run 'make userland' first" >&2
    exit 1
fi


# ----------------------------------------------------------------------
# Destination setup
# ----------------------------------------------------------------------

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

    # Only delete a partially-created disk image.
    # Never delete the user's ROOTFS_DIR.
    if [[ $directory_mode -eq 0 && $completed -eq 0 ]]; then
        rm -f -- "$image" "$ready_file"
    fi
}

trap cleanup EXIT


if [[ $directory_mode -eq 1 ]]; then
    # --------------------------------------------------------------
    # Directory mode
    # --------------------------------------------------------------

    mkdir -p "$mount_dir"

    if [[ $reset -eq 1 ]]; then
        # Remove everything, including dotfiles, without deleting the
        # destination directory itself.
        find "$mount_dir" \
            -mindepth 1 \
            -maxdepth 1 \
            -exec rm -rf -- {} +
    fi

    echo "building rootfs directory: $mount_dir"

else
    # --------------------------------------------------------------
    # Disk image mode
    # --------------------------------------------------------------

    mkdir -p "$(dirname -- "$image")" "$mount_dir"

    rm -f -- "$image"
    rm -f -- "$ready_file"

    dd if=/dev/zero \
       of="$image" \
       bs=1M \
       count="${IMAGE_SIZE_MB:-256}" \
       status=none

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

    map_output=$(sudo "$kpartx_bin" -av "$image")

    mapper=$(awk '/add map/ { print $3; exit }' <<<"$map_output")

    if [[ -z $mapper ]]; then
        echo "kpartx did not create a partition mapper" >&2
        exit 1
    fi

    device="/dev/mapper/$mapper"

    sudo "$mkfs_fat_bin" -F 32 "$device" >/dev/null

    sudo mount \
        -o "uid=$(id -u),gid=$(id -g),umask=022" \
        "$device" \
        "$mount_dir"

    mounted=1
fi


# ----------------------------------------------------------------------
# Populate root filesystem
#
# Everything below here works identically whether mount_dir is:
#
#   * a mounted FAT filesystem
#   * build/rootfs
#   * /tmp/rootfs
#   * any other normal directory
# ----------------------------------------------------------------------

cp -a "$seed_dir"/. "$mount_dir"/

mkdir -p \
    "$mount_dir/bin" \
    "$mount_dir/sbin"


# ----------------------------------------------------------------------
# Userland programs
# ----------------------------------------------------------------------

while IFS= read -r program; do
    [[ -z $program || $program == \#* ]] && continue

    [[ -f $stage_dir/$program ]] || {
        echo "staged program missing: $program" >&2
        exit 1
    }

    cp "$stage_dir/$program" "$mount_dir/bin/$program"

done < "$root/image/manifest.txt"


# ----------------------------------------------------------------------
# init
# ----------------------------------------------------------------------

[[ -f $stage_dir/init ]] || {
    echo "staged init missing: run 'make userland' first" >&2
    exit 1
}

cp "$stage_dir/init" "$mount_dir/sbin/init"


# ----------------------------------------------------------------------
# TinyCC runtime
# ----------------------------------------------------------------------

if [[ -d $tcc_runtime/usr ]]; then
    mkdir -p "$mount_dir/usr"
    cp -R "$tcc_runtime/usr"/. "$mount_dir/usr/"
fi


# ----------------------------------------------------------------------
# TinyCC source
# ----------------------------------------------------------------------

if [[ -f $tcc_source/tcc.c && -f $tcc_source/tccdefs_.h ]]; then
    mkdir -p \
        "$mount_dir/usr/src/tinycc/include" \
        "$mount_dir/usr/src/tinycc-kyleos"

    for source_file in \
        "$tcc_source"/*.c \
        "$tcc_source"/*.h \
        "$tcc_source"/*.def
    do
        [[ -f $source_file ]] &&
            cp "$source_file" "$mount_dir/usr/src/tinycc/"
    done

    cp -R \
        "$tcc_source/include"/. \
        "$mount_dir/usr/src/tinycc/include/"

    cp \
        "$tcc_port"/*.c \
        "$tcc_port"/*.h \
        "$mount_dir/usr/src/tinycc-kyleos/"
fi


# ----------------------------------------------------------------------
# KyleOS userspace source
# ----------------------------------------------------------------------

if [[ -d $core_source || -d $progs_source ]]; then
    mkdir -p \
        "$mount_dir/usr/src/kyleos-userland/core" \
        "$mount_dir/usr/src/kyleos-userland/progs"

    if [[ -d $core_source ]]; then
        cp \
            "$core_source"/*.c \
            "$mount_dir/usr/src/kyleos-userland/core/" \
            2>/dev/null || true
    fi

    if [[ -d $progs_source ]]; then
        cp \
            "$progs_source"/*.c \
            "$mount_dir/usr/src/kyleos-userland/progs/" \
            2>/dev/null || true
    fi
fi


# ----------------------------------------------------------------------
# Doom
# ----------------------------------------------------------------------

if [[ -f $doom_binary ]]; then
    mkdir -p \
        "$mount_dir/usr/bin" \
        "$mount_dir/usr/share/doom"

    cp "$doom_binary" "$mount_dir/usr/bin/doom"

    if [[ -f $doom_wad ]]; then
        cp \
            "$doom_wad" \
            "$mount_dir/usr/share/doom/doom.wad"
    else
        echo \
            "warning: Doom binary installed without a WAD; set DOOM_WAD=/path/to/doom.wad" \
            >&2
    fi
fi


# ----------------------------------------------------------------------
# Lua
# ----------------------------------------------------------------------

if [[ -f $lua_binary ]]; then
    mkdir -p \
        "$mount_dir/usr/bin" \
        "$mount_dir/usr/share/lua"

    cp "$lua_binary" "$mount_dir/usr/bin/lua"

    cp \
        "$root/extras/lua/examples"/*.lua \
        "$mount_dir/usr/share/lua/"
fi


# ----------------------------------------------------------------------
# Bible text
# ----------------------------------------------------------------------

if [[ -f $bible_text ]]; then
    mkdir -p "$mount_dir/usr/share/text"

    cp \
        "$bible_text" \
        "$mount_dir/usr/share/text/bible.txt"
fi


# ----------------------------------------------------------------------
# Finish
# ----------------------------------------------------------------------

sync

if [[ $directory_mode -eq 0 ]]; then
    touch "$ready_file"
fi

completed=1

if [[ $directory_mode -eq 1 ]]; then
    echo "rootfs created: $mount_dir"
fi
