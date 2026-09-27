#!/usr/bin/env python3
"""Generate the fmv_colortest grayscale-level test pattern as a PNG.

The 384x240 output replaces reference.png directly and is accepted by
create_movie_file.sh.
"""

import argparse
import struct
import zlib
from pathlib import Path


WIDTH = 384
MPEG_INPUT_HEIGHT = 240
TEST_LEVELS = (0, 16, 32, 64, 128, 192, 223, 235, 239, 255)


def chunk(kind: bytes, data: bytes) -> bytes:
    return struct.pack(">I", len(data)) + kind + data + struct.pack(">I", zlib.crc32(kind + data) & 0xFFFFFFFF)


def write_png(output: Path) -> None:
    # Mirror decodeRle()'s bar indices. The direct CLUT has calibrated levels.
    row = b"".join(
        bytes((level, level, level))
        for x in range(WIDTH)
        for level in (TEST_LEVELS[(x * len(TEST_LEVELS)) // WIDTH],)
    )
    scanlines = b"".join(b"\x00" + row for _ in range(MPEG_INPUT_HEIGHT))
    png = b"\x89PNG\r\n\x1a\n"
    png += chunk(b"IHDR", struct.pack(">IIBBBBB", WIDTH, MPEG_INPUT_HEIGHT, 8, 2, 0, 0, 0))
    png += chunk(b"IDAT", zlib.compress(scanlines, level=9))
    png += chunk(b"IEND", b"")
    output.write_bytes(png)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, default=Path("reference.png"), help="PNG to create (default: reference.png)")
    args = parser.parse_args()
    write_png(args.output)
    print(f"Wrote {args.output} ({WIDTH}x{MPEG_INPUT_HEIGHT})")


if __name__ == "__main__":
    main()
