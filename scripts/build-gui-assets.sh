#!/usr/bin/env bash
set -euo pipefail

root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
source_dir="$root/extras/desktop/art"
output_dir="$root/image/rootfs/usr/share/gui"
converter="$root/scripts/png-to-kimg.py"
work_dir=$(mktemp -d /tmp/kyleos-gui-assets.XXXXXX)

cleanup() {
    case $work_dir in
        /tmp/kyleos-gui-assets.*) rm -rf -- "$work_dir" ;;
        *) echo "refusing unsafe temporary path: $work_dir" >&2 ;;
    esac
}
trap cleanup EXIT

for tool in convert identify python3; do
    command -v "$tool" >/dev/null || {
        echo "missing host tool: $tool" >&2
        exit 1
    }
done

[[ $(identify -format '%wx%h' "$source_dir/kyleos-icons.png") == 1254x1254 ]] || {
    echo "icon sheet must be 1254x1254" >&2
    exit 1
}

mkdir -p "$output_dir"
convert "$source_dir/kyleos-hills.png" -resize 1280x720! \
    "$work_dir/wallpaper.png"
python3 "$converter" "$work_dir/wallpaper.png" \
    "$output_dir/wallpaper.kimg"

convert "$source_dir/kyleos-folder.png" -trim +repage -resize 58x58 \
    -gravity center -background none -extent 64x64 "$work_dir/folder.png"
python3 "$converter" "$work_dir/folder.png" "$output_dir/folder.kimg"

names=(system-disk readme duke3d trash editor terminal mines solitaire calculator)
positions=(0+0 418+0 836+0 0+418 418+418 836+418 0+836 418+836 836+836)

for index in "${!names[@]}"; do
    name=${names[$index]}
    position=${positions[$index]}
    convert "$source_dir/kyleos-icons.png" \
        -crop "418x418+$position" +repage -trim +repage \
        -resize 58x58 -gravity center -background none -extent 64x64 \
        "$work_dir/$name.png"
    python3 "$converter" "$work_dir/$name.png" "$output_dir/$name.kimg"
done

echo "KyleOS GUI assets written to $output_dir"
