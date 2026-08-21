#!/usr/bin/env python3
"""Build a compact KyleOS alpha-atlas font from a TrueType/OpenType font."""

import argparse
import struct
from pathlib import Path

from PIL import ImageFont


MAGIC = b"KFN1"
VERSION = 1
GLYPH_COUNT = 256
ATLAS_WIDTH = 512
PADDING = 1


def glyph_bitmap(font: ImageFont.FreeTypeFont, codepoint: int):
    if codepoint < 32 or 127 <= codepoint < 160:
        return 0, 0, 0, 0, b""
    character = chr(codepoint)
    left, top, right, bottom = font.getbbox(character)
    width = max(0, right - left)
    height = max(0, bottom - top)
    if width == 0 or height == 0:
        return left, top, width, height, b""
    mask = font.getmask(character, mode="L")
    return left, top, width, height, bytes(mask)


def build_font(source: Path, output: Path, size: int, line_height: int):
    font = ImageFont.truetype(str(source), size=size)
    ascent, _ = font.getmetrics()
    records = []
    bitmaps = []
    cursor_x = PADDING
    cursor_y = PADDING
    row_height = 0

    for codepoint in range(GLYPH_COUNT):
        left, top, width, height, bitmap = glyph_bitmap(font, codepoint)
        advance = int(round(font.getlength(chr(codepoint))))
        if codepoint < 32 or 127 <= codepoint < 160:
            advance = 0
        if codepoint == 32:
            width = height = 0
            bitmap = b""
        if cursor_x + width + PADDING > ATLAS_WIDTH:
            cursor_x = PADDING
            cursor_y += row_height + PADDING
            row_height = 0
        records.append((cursor_x, cursor_y, width, height,
                        left, top, max(0, advance), 0))
        bitmaps.append((cursor_x, cursor_y, width, height, bitmap))
        cursor_x += width + PADDING
        row_height = max(row_height, height)

    atlas_height = cursor_y + row_height + PADDING
    atlas = bytearray(ATLAS_WIDTH * atlas_height)
    for x, y, width, height, bitmap in bitmaps:
        for row in range(height):
            source_offset = row * width
            target_offset = (y + row) * ATLAS_WIDTH + x
            atlas[target_offset:target_offset + width] = \
                bitmap[source_offset:source_offset + width]

    output.parent.mkdir(parents=True, exist_ok=True)
    with output.open("wb") as destination:
        destination.write(struct.pack(
            "<4s7I", MAGIC, VERSION, GLYPH_COUNT, ATLAS_WIDTH,
            atlas_height, line_height, ascent, len(atlas)))
        for record in records:
            destination.write(struct.pack("<HHHHhhhh", *record))
        destination.write(atlas)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("source", type=Path)
    parser.add_argument("output", type=Path)
    parser.add_argument("--size", type=int, required=True)
    parser.add_argument("--line-height", type=int, default=16)
    arguments = parser.parse_args()
    build_font(arguments.source, arguments.output,
               arguments.size, arguments.line_height)


if __name__ == "__main__":
    main()
