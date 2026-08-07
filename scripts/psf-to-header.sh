#!/usr/bin/env sh
set -eu

input=$1
output=$2

# PSF1: 4-byte header followed by 256 glyphs of 16 bytes.  The font is
# deliberately kept as a compressed source asset; this emits a C header in
# the build tree at compile time.
{
    printf '%s\n' '#ifndef KYLEOS_TERMINAL_FONT_H'
    printf '%s\n' '#define KYLEOS_TERMINAL_FONT_H'
    printf '%s\n' 'static const uint8_t terminal_font[4096] = {'
    gzip -cd "$input" | dd bs=1 skip=4 count=4096 status=none | \
        od -An -v -tu1 | awk '{ for (i = 1; i <= NF; i++) { printf "0x%02x,", $i; if (++n % 16 == 0) printf "\n"; } }'
    printf '%s\n' '};'
    printf '%s\n' '#endif'
} > "$output"
