#!/usr/bin/env bash
set -euo pipefail

root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
asset_dir=${QUAKE_ASSET_DIR:-"$root/assets/quake/id1"}
pak_file="$asset_dir/pak0.pak"

archive_url=https://www.gamers.org/pub/idgames2/idstuff/quake/quake106.zip
archive_sha256=ec6c9d34b1ae0252ac0066045b6611a7919c2a0d78a3a66d9387a8f597553239
pak_sha256=35a9c55e5e5a284a159ad2a62e0e8def23d829561fe2f54eb402dbc0a9a946af

data_only=0

usage() {
    cat <<'EOF'
usage: scripts/setup-quake-shareware.sh [--data-only]

Downloads and verifies Quake 1.06 shareware, installs pak0.pak, initializes
the Quake source submodule, and builds a new KyleOS image.

  --data-only  Install pak0.pak without building Quake or recreating the image.

Set QUAKE_ASSET_DIR to install the PAK somewhere other than assets/quake/id1.
Set QUAKE_SHAREWARE_ARCHIVE to reuse an existing quake106.zip download.
EOF
}

case ${1:-} in
    "") ;;
    --data-only) data_only=1 ;;
    -h|--help)
        usage
        exit 0
        ;;
    *)
        usage >&2
        exit 2
        ;;
esac

if [[ $# -gt 1 ]]; then
    usage >&2
    exit 2
fi

find_extractor() {
    if command -v lhasa >/dev/null 2>&1; then
        command -v lhasa
    elif command -v lha >/dev/null 2>&1; then
        command -v lha
    else
        return 1
    fi
}

if [[ -f $pak_file ]]; then
    echo "keeping existing Quake data: $pak_file"
else
    for tool in cp unzip sha256sum install mktemp; do
        command -v "$tool" >/dev/null 2>&1 || {
            echo "missing required tool: $tool" >&2
            exit 1
        }
    done
    if [[ -z ${QUAKE_SHAREWARE_ARCHIVE:-} ]]; then
        command -v curl >/dev/null 2>&1 || {
            echo "missing required tool: curl" >&2
            exit 1
        }
    fi

    extractor=$(find_extractor) || {
        echo "missing LHA extractor" >&2
        echo "Debian/Ubuntu: sudo apt install lhasa" >&2
        echo "Fedora:        sudo dnf install lhasa" >&2
        echo "Arch Linux:    sudo pacman -S lhasa" >&2
        exit 1
    }

    work_dir=$(mktemp -d "${TMPDIR:-/tmp}/kyleos-quake-shareware.XXXXXX")
    touch "$work_dir/.kyleos-quake-setup-temp"
    cleanup() {
        cleanup_dir=${work_dir:-}
        [[ -n $cleanup_dir && -d $cleanup_dir && ! -L $cleanup_dir &&
           -f $cleanup_dir/.kyleos-quake-setup-temp ]] || return
        case $cleanup_dir in
            "${TMPDIR:-/tmp}"/kyleos-quake-shareware.??????) ;;
            *) return ;;
        esac
        rm -rf -- "$cleanup_dir"
    }
    trap cleanup EXIT

    archive="$work_dir/quake106.zip"
    extract_dir="$work_dir/extracted"
    mkdir -p "$extract_dir"

    if [[ -n ${QUAKE_SHAREWARE_ARCHIVE:-} ]]; then
        [[ -f $QUAKE_SHAREWARE_ARCHIVE ]] || {
            echo "archive not found: $QUAKE_SHAREWARE_ARCHIVE" >&2
            exit 1
        }
        cp "$QUAKE_SHAREWARE_ARCHIVE" "$archive"
        echo "using existing archive: $QUAKE_SHAREWARE_ARCHIVE"
    else
        echo "downloading Quake 1.06 shareware..."
        curl --fail --location --retry 3 --output "$archive" "$archive_url"
    fi
    printf '%s  %s\n' "$archive_sha256" "$archive" | sha256sum --check --status || {
        echo "Quake archive checksum mismatch" >&2
        exit 1
    }

    unzip -q "$archive" resource.1 -d "$work_dir"
    (
        cd "$extract_dir"
        "$extractor" xq "$work_dir/resource.1" id1/pak0.pak
    )

    extracted_pak="$extract_dir/id1/pak0.pak"
    [[ -f $extracted_pak ]] || {
        echo "shareware archive did not contain id1/pak0.pak" >&2
        exit 1
    }
    printf '%s  %s\n' "$pak_sha256" "$extracted_pak" | sha256sum --check --status || {
        echo "pak0.pak checksum mismatch" >&2
        exit 1
    }

    install -D -m 0644 "$extracted_pak" "$pak_file"
    echo "installed shareware data: $pak_file"
fi

if [[ $data_only -eq 1 ]]; then
    exit 0
fi

for tool in git make; do
    command -v "$tool" >/dev/null 2>&1 || {
        echo "missing required tool: $tool" >&2
        exit 1
    }
done

sysroot=${SYSROOT:-/tmp/z}
if [[ ! -f $sysroot/lib/libc.a || ! -f $sysroot/lib/libm.a ]]; then
    echo "KyleOS Newlib sysroot is missing from $sysroot" >&2
    echo "Build it first with: make toolchain" >&2
    exit 1
fi

if [[ ! -e $root/extras/quake/.git ]]; then
    echo "initializing the Quake source submodule..."
    git -C "$root" submodule update --init extras/quake
fi

echo "building Quake and recreating the KyleOS image..."
QUAKE_DATA_DIR="$asset_dir" SYSROOT="$sysroot" make -C "$root" quake-image

cat <<'EOF'

Quake setup complete. Boot KyleOS with:

    make test

Then run this inside KyleOS:

    quake
EOF
