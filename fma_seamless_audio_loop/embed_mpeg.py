#!/usr/bin/env python3
"""Wrap an MP2 elementary stream in 2,304-byte MPEG Program Stream sectors."""

from __future__ import annotations

import argparse
from pathlib import Path
import re
import subprocess
import sys


SECTOR_SIZE = 2304
MP2_BITRATES_MPEG1_LAYER2 = (
    0, 32, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320, 384,
)
MP2_SAMPLE_RATES_MPEG1 = (44100, 48000, 32000)


def timestamp_90k(data: bytes) -> int:
    return (((data[0] >> 1) & 7) << 30) | (data[1] << 22) | ((data[2] & 0xFE) << 14) | (data[3] << 7) | ((data[4] & 0xFE) >> 1)


def write_timestamp(value: int) -> bytes:
    return bytes((0x21 | ((value >> 29) & 0x0E), (value >> 22) & 0xFF,
                  0x01 | ((value >> 14) & 0xFE), (value >> 7) & 0xFF,
                  0x01 | ((value << 1) & 0xFE)))


def guard_pack(stream: bytes) -> bytes:
    """Ensure the terminal audio PES is in a pack-led, padded sector."""
    last_pts = None
    last_scr = None
    last_scr_pts = None
    last_pes_offset = None
    last_pes_end = None
    for sector_base in range(0, len(stream), SECTOR_SIZE):
        # ffmpeg's fixed-size Program Stream packets place a PES header at
        # offset 0, 12 (after a pack), or 27 (after pack/system header).
        for relative in (0, 12, 27):
            offset = sector_base + relative
            if (offset + 11 <= len(stream) and
                    stream[offset:offset + 4] == b"\x00\x00\x01\xc0" and
                    (stream[offset + 6] & 0xF0) == 0x20):
                pes_end = offset + 6 + int.from_bytes(stream[offset + 4:offset + 6], "big")
                if pes_end <= sector_base + SECTOR_SIZE:
                    last_pts = timestamp_90k(stream[offset + 6:offset + 11])
                    last_pes_offset = offset
                    last_pes_end = pes_end
        if stream[sector_base:sector_base + 4] == b"\x00\x00\x01\xba":
            scr = timestamp_90k(stream[sector_base + 4:sector_base + 9])
            for relative in (12, 27):
                offset = sector_base + relative
                if (stream[offset:offset + 4] == b"\x00\x00\x01\xc0" and
                        (stream[offset + 6] & 0xF0) == 0x20):
                    last_scr = scr
                    last_scr_pts = timestamp_90k(stream[offset + 6:offset + 11])
    if last_pts is None:
        raise ValueError("cannot add guard pack: stream has no audio PTS")
    # MPEG-1 pack header (12 bytes) followed by a padding stream packet.
    # A terminal PES which starts a sector is the one layout that the CD-i
    # decoder does not reliably continue into the next PCL.  Put a pack in
    # front of it, preserving the normal PTS-to-SCR lead of this stream.
    if (last_pes_offset % SECTOR_SIZE == 0 and last_scr is not None and
            last_scr_pts is not None):
        lead = last_scr_pts - last_scr
        pack = (b"\x00\x00\x01\xba" + write_timestamp(last_pts - lead) +
                b"\x80\x04\x23")
        sector_end = last_pes_offset + SECTOR_SIZE
        remaining = sector_end - (last_pes_end + len(pack))
        if remaining >= 6:
            padding = (b"\x00\x00\x01\xbe" +
                       (remaining - 6).to_bytes(2, "big") +
                       b"\xff" * (remaining - 6))
            return (stream[:last_pes_offset] + pack +
                    stream[last_pes_offset:last_pes_end] + padding +
                    stream[sector_end:])

    pack = b"\x00\x00\x01\xba" + write_timestamp(last_pts + 1) + b"\x80\x04\x23"
    sector_end = ((last_pes_end + SECTOR_SIZE - 1) // SECTOR_SIZE) * SECTOR_SIZE
    remaining = sector_end - last_pes_end
    if remaining >= 18:
        padding_size = remaining - 18
        padding = b"\x00\x00\x01\xbe" + padding_size.to_bytes(2, "big") + b"\xff" * padding_size
        return stream[:last_pes_end] + pack + padding + stream[sector_end:]
    padding = b"\x00\x00\x01\xbe\x08\xee" + b"\xff" * 2286
    return stream + pack + padding


def mp2_period_90k(data: bytes) -> int:
    """Return the exact rounded 90 kHz duration of an MPEG-1 Layer II ES."""
    offset = 0
    frame_count = 0
    sample_rate = 0

    while offset < len(data):
        if offset + 4 > len(data):
            raise ValueError("truncated MP2 frame header")
        header = int.from_bytes(data[offset : offset + 4], "big")
        version = (header >> 19) & 0x3
        layer = (header >> 17) & 0x3
        bitrate_index = (header >> 12) & 0xF
        sample_rate_index = (header >> 10) & 0x3
        padding = (header >> 9) & 0x1
        if (header >> 21) != 0x7FF or version != 3 or layer != 2:
            raise ValueError(f"invalid MPEG-1 Layer II frame at byte {offset}")
        if bitrate_index == 0 or bitrate_index == 15 or sample_rate_index == 3:
            raise ValueError(f"invalid MP2 bitrate/sample rate at byte {offset}")

        bitrate = MP2_BITRATES_MPEG1_LAYER2[bitrate_index] * 1000
        this_rate = MP2_SAMPLE_RATES_MPEG1[sample_rate_index]
        if sample_rate and this_rate != sample_rate:
            raise ValueError("MP2 sample rate changes within loop")
        sample_rate = this_rate
        frame_size = (144 * bitrate) // sample_rate + padding
        if offset + frame_size > len(data):
            raise ValueError("truncated MP2 frame")
        offset += frame_size
        frame_count += 1

    if not frame_count:
        raise ValueError("MP2 stream has no frames")
    ticks_numerator = frame_count * 1152 * 90000
    return (ticks_numerator + sample_rate // 2) // sample_rate


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("input", type=Path, help="MP2 elementary stream")
    parser.add_argument("output", type=Path, help="generated C source")
    parser.add_argument("--name", default="lostride_mpg", help="C symbol base for the embedded stream")
    parser.add_argument("--guard-pack", action="store_true", help="append a packet-free SCR guard sector")
    args = parser.parse_args()
    if not args.input.is_file():
        parser.error(f"input does not exist: {args.input}")
    if not re.fullmatch(r"[A-Za-z_][A-Za-z0-9_]*", args.name):
        parser.error("--name must be a C identifier")
    try:
        period_90k = mp2_period_90k(args.input.read_bytes())
    except ValueError as exc:
        parser.error(str(exc))

    result = subprocess.run(
        ["ffmpeg", "-y", "-v", "error", "-i", str(args.input),
         "-c:a", "copy", "-muxpreload", "0.44", "-f", "mpeg",
         "-packetsize", str(SECTOR_SIZE), "pipe:1"],
        stdout=subprocess.PIPE, stderr=subprocess.PIPE, check=False,
    )
    if result.returncode:
        print(f"embed_mpeg: ffmpeg failed: {result.stderr.decode(errors='replace').strip()}", file=sys.stderr)
        return 2
    if not result.stdout or len(result.stdout) % SECTOR_SIZE:
        print("embed_mpeg: ffmpeg did not produce whole 2,304-byte Program Stream sectors", file=sys.stderr)
        return 2
    if args.guard_pack:
        try:
            result_stream = guard_pack(result.stdout)
        except ValueError as exc:
            print(f"embed_mpeg: {exc}", file=sys.stderr)
            return 2
    else:
        result_stream = result.stdout

    lines = ["/* Generated by embed_mpeg.py; do not edit manually. */\n", f"unsigned char {args.name}[] = {{\n"]
    for offset in range(0, len(result_stream), 12):
        values = ", ".join(f"0x{byte:02x}" for byte in result_stream[offset : offset + 12])
        lines.append(f"  {values},\n")
    lines.extend((
        "};\n",
        f"unsigned long {args.name}_len = {len(result_stream)};\n",
        f"unsigned long {args.name}_period_90k = {period_90k};\n",
    ))
    args.output.write_text("".join(lines))
    print(f"embedded {args.input} as {len(result_stream) // SECTOR_SIZE} MPEG sectors "
          f"(period {period_90k} ticks) in {args.output}", file=sys.stderr)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
