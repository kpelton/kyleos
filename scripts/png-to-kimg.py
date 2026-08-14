#!/usr/bin/env python3
"""Convert a PNG into KyleOS's tiny little-endian ARGB KIMG format."""

import argparse
import struct
import subprocess
from pathlib import Path


def command_output(*arguments: str) -> bytes:
    return subprocess.run(arguments, check=True, stdout=subprocess.PIPE).stdout


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("input", type=Path)
    parser.add_argument("output", type=Path)
    arguments = parser.parse_args()

    dimensions = command_output(
        "identify", "-format", "%w %h", str(arguments.input)
    ).decode("ascii").split()
    width, height = (int(value) for value in dimensions)
    rgba = command_output("convert", str(arguments.input), "rgba:-")
    if len(rgba) != width * height * 4:
        raise SystemExit("ImageMagick returned an unexpected pixel count")

    pixels = bytearray(len(rgba))
    opaque = True
    for offset in range(0, len(rgba), 4):
        red, green, blue, alpha = rgba[offset : offset + 4]
        if alpha != 255:
            opaque = False
        pixels[offset : offset + 4] = struct.pack(
            "<I", (alpha << 24) | (red << 16) | (green << 8) | blue
        )

    arguments.output.parent.mkdir(parents=True, exist_ok=True)
    with arguments.output.open("wb") as output:
        output.write(struct.pack("<4sIIII", b"KIMG", 1, width, height,
                                 1 if opaque else 0))
        output.write(pixels)


if __name__ == "__main__":
    main()
