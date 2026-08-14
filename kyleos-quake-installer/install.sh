#!/usr/bin/env bash
set -euo pipefail

usage() {
    cat <<'EOF'
Usage: sudo ./install.sh [--yes] /mounted/kyleos/root

Installs Quake and its shareware PAK onto an already-mounted KyleOS drive.
Existing save files are preserved. If the mounted drive contains the GRUB
kernel at /boot/kernel.bin, the bundled fixed kernel is installed too.
EOF
}

assume_yes=0
if [[ ${1:-} == "--yes" ]]; then
    assume_yes=1
    shift
fi
if [[ $# -ne 1 ]]; then
    usage >&2
    exit 2
fi

bundle_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
root_arg=$1

[[ ! -L $root_arg ]] || {
    echo "refusing symbolic-link target: $root_arg" >&2
    exit 1
}
root_dir=$(realpath -e -- "$root_arg")
[[ -d $root_dir && $root_dir != / ]] || {
    echo "unsafe root target: $root_dir" >&2
    exit 1
}
mountpoint -q "$root_dir" || {
    echo "not a mounted filesystem: $root_dir" >&2
    exit 1
}
[[ -w $root_dir ]] || {
    echo "mounted filesystem is not writable: $root_dir" >&2
    exit 1
}
if [[ ! -e $root_dir/bin/nushell && ! -e $root_dir/sbin/init ]]; then
    echo "target does not look like a KyleOS root filesystem: $root_dir" >&2
    exit 1
fi

(
    cd "$bundle_dir"
    sha256sum -c SHA256SUMS
)

echo
echo "KyleOS root: $root_dir"
echo "Will install:"
echo "  /usr/bin/quake"
echo "  /usr/share/quake/id1/pak0.pak"
if [[ -f $root_dir/boot/kernel.bin || -f $root_dir/boot/grub/grub.cfg ]]; then
    echo "  /boot/kernel.bin"
else
    echo "Kernel: separate boot filesystem detected or not mounted; skipping"
fi
echo "Existing *.sav files will not be removed."

if [[ $assume_yes -eq 0 ]]; then
    read -r -p "Install to this mounted drive? [y/N] " answer
    [[ $answer == y || $answer == Y ]] || {
        echo "cancelled"
        exit 1
    }
fi

install -D -m 755 \
    "$bundle_dir/payload/usr/bin/quake" \
    "$root_dir/usr/bin/quake"
install -D -m 644 \
    "$bundle_dir/payload/usr/share/quake/id1/pak0.pak" \
    "$root_dir/usr/share/quake/id1/pak0.pak"

if [[ -f $root_dir/boot/kernel.bin || -f $root_dir/boot/grub/grub.cfg ]]; then
    install -D -m 755 \
        "$bundle_dir/payload/boot/kernel.bin" \
        "$root_dir/boot/kernel.bin"
fi

sync
echo
echo "Quake installation complete. Unmount the drive before disconnecting it."
echo "Boot KyleOS and run: quake"
