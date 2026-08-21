#!/usr/bin/env bash
set -euo pipefail

root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
source_dir="$root/extras/desktop/art"
output_dir="$root/image/rootfs/usr/share/gui"
converter="$root/scripts/png-to-kimg.py"
font_converter="$root/scripts/ttf-to-kfont.py"
work_dir=$(mktemp -d /tmp/kyleos-gui-assets.XXXXXX)

cleanup() {
    case $work_dir in
        /tmp/kyleos-gui-assets.*) rm -rf -- "$work_dir" ;;
        *) echo "refusing unsafe temporary path: $work_dir" >&2 ;;
    esac
}
trap cleanup EXIT

for tool in convert identify python3 fc-match; do
    command -v "$tool" >/dev/null || {
        echo "missing host tool: $tool" >&2
        exit 1
    }
done
python3 -c 'import PIL' >/dev/null || {
    echo "missing host Python module: Pillow" >&2
    exit 1
}

[[ $(identify -format '%wx%h' "$source_dir/kyleos-icons.png") == 1254x1254 ]] || {
    echo "icon sheet must be 1254x1254" >&2
    exit 1
}

mkdir -p "$output_dir"
convert "$source_dir/kyleos-hills.png" -resize 1280x720! \
    "$work_dir/wallpaper.png"
python3 "$converter" "$work_dir/wallpaper.png" \
    "$output_dir/wallpaper.kimg"

# Keep the original 4:3 source untouched. The runtime asset is center-cropped
# to the default widescreen framebuffer without distorting Aqua's arcs.
convert "$source_dir/aqua-blue.jpg" -resize 1280x720^ -gravity center \
    -extent 1280x720 "$work_dir/aqua-wallpaper.png"
python3 "$converter" "$work_dir/aqua-wallpaper.png" \
    "$output_dir/aqua-wallpaper.kimg"

convert "$source_dir/kyleos-folder.png" -trim +repage -resize 58x58 \
    -gravity center -background none -extent 64x64 "$work_dir/folder.png"
python3 "$converter" "$work_dir/folder.png" "$output_dir/folder.kimg"

names=(sysmon readme duke3d trash editor terminal mines solitaire calculator)
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

for asset in system-disk citybuilder paint cide; do
    case $asset in
        system-disk) source_name=home ;;
        citybuilder) source_name=city ;;
        paint) source_name=paint ;;
        cide) source_name=cide ;;
    esac
    convert "$source_dir/kyleos-$source_name.png" -trim +repage \
        -resize 58x58 -gravity center -background none -extent 64x64 \
        "$work_dir/$asset.png"
    python3 "$converter" "$work_dir/$asset.png" "$output_dir/$asset.kimg"
done

aqua_font=$(fc-match -f '%{file}' 'DejaVu Sans')
win95_font=$(fc-match -f '%{file}' 'DejaVu Sans Condensed')
mono_font=$(fc-match -f '%{file}' 'DejaVu Sans Mono')
for font in "$aqua_font" "$win95_font" "$mono_font"; do
    [[ -f $font ]] || {
        echo "missing required DejaVu font: $font" >&2
        exit 1
    }
done
python3 "$font_converter" "$aqua_font" "$output_dir/font-aqua.kfont" \
    --size 13 --line-height 16
python3 "$font_converter" "$win95_font" "$output_dir/font-win95.kfont" \
    --size 12 --line-height 16
python3 "$font_converter" "$mono_font" "$output_dir/font-mono.kfont" \
    --size 13 --line-height 16

echo "KyleOS GUI assets written to $output_dir"
