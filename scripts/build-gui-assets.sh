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

convert "$source_dir/kyleos-tetris.png" -trim +repage -resize 58x58 \
    -gravity center -background none -extent 64x64 "$work_dir/tetris.png"
python3 "$converter" "$work_dir/tetris.png" "$output_dir/tetris.kimg"

# A compact spreadsheet document: green title band, white worksheet and a
# high-contrast grid that remains recognizable at Aqua Dock zoom sizes.
convert -size 64x64 xc:none \
    -fill '#173f35' -stroke '#0b261f' -strokewidth 2 \
    -draw 'roundrectangle 5,4 59,60 8,8' \
    -fill '#eaf7ef' -stroke '#eaf7ef' -strokewidth 1 \
    -draw 'rectangle 10,18 54,54' \
    -fill '#45a56f' -stroke '#45a56f' \
    -draw 'rectangle 10,10 54,18' \
    -stroke '#6f9c82' -strokewidth 1 \
    -draw 'line 21,18 21,54 line 32,18 32,54 line 43,18 43,54 line 10,27 54,27 line 10,36 54,36 line 10,45 54,45' \
    -depth 8 \
    "$work_dir/ksheet.png"
python3 "$converter" "$work_dir/ksheet.png" "$output_dir/ksheet.kimg"

# Aqua-style Help orb: a deep blue rim, cyan glass highlight, and a clear
# white question mark that stays legible at normal and magnified Dock sizes.
convert -size 64x64 xc:none \
    -fill '#173d78' -stroke '#0a2c63' -strokewidth 2 \
    -draw 'circle 32,32 59,32' \
    -fill '#2f86d7' -stroke '#78bff3' -strokewidth 1 \
    -draw 'circle 32,30 55,30' \
    -fill '#94d8ff' -stroke none \
    -draw 'ellipse 27,20 15,9 190,350' \
    -fill none -stroke '#ffffff' -strokewidth 6 \
    -draw "path 'M 22,23 C 22,13 43,12 43,24 C 43,32 32,31 32,40'" \
    -fill '#ffffff' -stroke none \
    -draw 'circle 32,49 35,49' \
    -depth 8 \
    "$work_dir/help.png"
python3 "$converter" "$work_dir/help.png" "$output_dir/help.kimg"

# KyleFPS icon: a tiny red target at the end of a blue steel corridor.
convert -size 64x64 xc:none \
    -fill '#172337' -stroke '#08111f' -strokewidth 2 \
    -draw 'roundrectangle 3,3 61,61 9,9' \
    -fill '#334b68' -stroke none \
    -draw 'polygon 7,10 27,25 27,43 7,56' \
    -draw 'polygon 57,10 37,25 37,43 57,56' \
    -fill '#27313d' \
    -draw 'polygon 7,56 27,43 37,43 57,56' \
    -fill '#d4443f' -stroke '#7e1f28' -strokewidth 1 \
    -draw 'rectangle 27,23 37,39' \
    -fill '#f4c15a' -stroke none \
    -draw 'rectangle 30,26 32,28 rectangle 35,26 37,28' \
    -fill none -stroke '#f2f5f7' -strokewidth 1 \
    -draw 'line 32,14 32,22 line 32,40 32,48 line 19,32 26,32 line 38,32 45,32' \
    -depth 8 \
    "$work_dir/fps.png"
python3 "$converter" "$work_dir/fps.png" "$output_dir/fps.kimg"

# Doom Aqua icon: an original, clean-room fiery portal motif. The engine is
# open source, but no proprietary WAD artwork is copied into the system image.
convert -size 64x64 xc:none \
    -fill '#17100f' -stroke '#080504' -strokewidth 2 \
    -draw 'roundrectangle 3,3 61,61 9,9' \
    -fill '#67241b' -stroke '#bd4a24' -strokewidth 2 \
    -draw 'ellipse 32,33 20,24 0,360' \
    -fill '#e56b22' -stroke '#ffad35' -strokewidth 1 \
    -draw 'ellipse 32,33 14,19 0,360' \
    -fill '#271716' -stroke '#130a09' -strokewidth 1 \
    -draw 'polygon 19,51 23,29 29,21 35,21 41,29 45,51 38,45 32,49 26,45' \
    -fill '#ffcf55' -stroke none \
    -draw 'circle 26,32 29,32 circle 35,32 38,32' \
    -depth 8 \
    "$work_dir/doom-aqua.png"
python3 "$converter" "$work_dir/doom-aqua.png" \
    "$output_dir/doom-aqua.kimg"

# Quake Aqua icon: an original violet slipgate floating over dark stone.
# No artwork from the commercial PAK files is embedded in the system image.
convert -size 64x64 xc:none \
    -fill '#171622' -stroke '#08070d' -strokewidth 2 \
    -draw 'roundrectangle 3,3 61,61 9,9' \
    -fill '#29283b' -stroke '#56516d' -strokewidth 1 \
    -draw 'polygon 8,53 15,18 25,11 39,11 49,18 56,53' \
    -fill '#1a1628' -stroke '#8068b5' -strokewidth 3 \
    -draw 'ellipse 32,34 15,22 0,360' \
    -fill '#493276' -stroke '#b493ed' -strokewidth 2 \
    -draw 'ellipse 32,34 9,16 0,360' \
    -fill '#d8c5ff' -stroke none \
    -draw 'ellipse 29,27 3,7 205,355' \
    -fill '#6d645b' -stroke '#28231f' -strokewidth 1 \
    -draw 'polygon 7,53 57,53 51,60 13,60' \
    -depth 8 \
    "$work_dir/quake-aqua.png"
python3 "$converter" "$work_dir/quake-aqua.png" \
    "$output_dir/quake-aqua.kimg"

block_colors=(2cc9e8 f2cf3f 9d62d5 5dcf69 e85a55 4f79d8 ef983c)
for index in "${!block_colors[@]}"; do
    number=$((index + 1))
    color=${block_colors[$index]}
    convert "$source_dir/kyleos-tetris-block.png" -trim +repage \
        -resize 16x16! -channel RGB -colorspace gray \
        -fill "#$color" -colorize 58% +channel \
        "$work_dir/tetris-block-$number.png"
    python3 "$converter" "$work_dir/tetris-block-$number.png" \
        "$output_dir/tetris-block-$number.kimg"
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
