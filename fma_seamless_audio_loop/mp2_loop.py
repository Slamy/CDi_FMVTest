#!/usr/bin/env python3
"""Extract a frame-aligned range from an MP2 elementary stream and loop it.

Examples:
    python mp2_loop.py input.mp2 loop.mp2 --start 100 --end 250 --loops 8
    python mp2_loop.py input.mp2 loop.mp2 --start-time 12.5 --duration 4 --loops 3
    python mp2_loop.py input.mp2 loop.mp2 --start 100 --end 250 --wrap-crossfade 0.08
"""

from __future__ import annotations

import argparse
import array
from dataclasses import dataclass
import math
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile


BITRATES = {
    3: (0, 32, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320, 384),
    2: (0, 8, 16, 24, 32, 40, 48, 56, 64, 80, 96, 112, 128, 144, 160),
    0: (0, 8, 16, 24, 32, 40, 48, 56, 64, 80, 96, 112, 128, 144, 160),
}
SAMPLE_RATES = {
    3: (44100, 48000, 32000),
    2: (22050, 24000, 16000),
    0: (11025, 12000, 8000),
}


@dataclass(frozen=True)
class Frame:
    offset: int
    size: int
    sample_rate: int
    bitrate: int
    channels: int


def skip_id3v2(data: bytes) -> int:
    if data[:3] != b"ID3" or len(data) < 10:
        return 0
    # ID3v2 sizes are four synchsafe bytes, excluding the ten-byte header.
    size_bytes = data[6:10]
    if any(byte & 0x80 for byte in size_bytes):
        raise ValueError("invalid ID3v2 synchsafe size")
    return 10 + sum(byte << (7 * (3 - index)) for index, byte in enumerate(size_bytes))


def parse_frame(data: bytes, offset: int) -> Frame:
    if offset + 4 > len(data):
        raise ValueError(f"truncated header at byte {offset}")
    header = int.from_bytes(data[offset : offset + 4], "big")
    if header >> 21 != 0x7FF:
        raise ValueError(f"no MPEG sync word at byte {offset}")

    version = (header >> 19) & 0b11
    layer = (header >> 17) & 0b11
    bitrate_index = (header >> 12) & 0b1111
    sample_rate_index = (header >> 10) & 0b11
    padding = (header >> 9) & 1
    channel_mode = (header >> 6) & 0b11
    if version == 1 or layer != 2:
        raise ValueError(f"frame at byte {offset} is not MPEG Layer II (MP2)")
    if bitrate_index in (0, 15) or sample_rate_index == 3:
        raise ValueError(f"invalid MP2 bitrate/sample-rate index at byte {offset}")

    bitrate = BITRATES[version][bitrate_index] * 1000
    sample_rate = SAMPLE_RATES[version][sample_rate_index]
    size = (144 * bitrate) // sample_rate + padding
    if offset + size > len(data):
        raise ValueError(f"truncated frame at byte {offset}: expected {size} bytes")
    return Frame(offset, size, sample_rate, bitrate, 1 if channel_mode == 3 else 2)


def index_frames(data: bytes) -> list[Frame]:
    offset = skip_id3v2(data)
    frames: list[Frame] = []
    while offset < len(data):
        # A trailing ID3v1 tag is metadata, not an audio frame.
        if data[offset : offset + 3] == b"TAG" and len(data) - offset == 128:
            break
        frame = parse_frame(data, offset)
        frames.append(frame)
        offset += frame.size
    if not frames:
        raise ValueError("no MP2 frames found")
    return frames


def crossfade_loop(segment: bytes, frame: Frame, loops: int, seconds: float) -> bytes:
    """Decode, overlap/mix loop joins, and encode the result back to MP2."""
    if not shutil.which("ffmpeg"):
        raise ValueError("--crossfade needs ffmpeg in PATH")

    decode = subprocess.run(
        ["ffmpeg", "-v", "error", "-f", "mp3", "-i", "pipe:0", "-f", "f32le", "-acodec", "pcm_f32le", "pipe:1"],
        input=segment,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        check=False,
    )
    if decode.returncode:
        raise ValueError(f"ffmpeg could not decode MP2: {decode.stderr.decode(errors='replace').strip()}")
    samples = array.array("f")
    samples.frombytes(decode.stdout)
    if sys.byteorder != "little":
        samples.byteswap()
    if len(samples) % frame.channels:
        raise ValueError("ffmpeg returned incomplete PCM samples")

    fade_frames = round(seconds * frame.sample_rate)
    source_frames = len(samples) // frame.channels
    if fade_frames < 1 or fade_frames >= source_frames:
        raise ValueError(
            f"crossfade must be at least one sample and shorter than the selected "
            f"segment ({source_frames / frame.sample_rate:.3f}s)"
        )

    mixed = array.array("f", samples)
    fade_values = fade_frames * frame.channels
    for _ in range(1, loops):
        join_start = len(mixed) - fade_values
        for sample_frame in range(fade_frames):
            phase = (sample_frame + 1) / fade_frames * math.pi / 2
            tail_gain, head_gain = math.cos(phase), math.sin(phase)
            base = sample_frame * frame.channels
            for channel in range(frame.channels):
                mixed[join_start + base + channel] = (
                    mixed[join_start + base + channel] * tail_gain
                    + samples[base + channel] * head_gain
                )
        mixed.extend(samples[fade_values:])

    if sys.byteorder != "little":
        mixed.byteswap()
    encode = subprocess.run(
        [
            "ffmpeg", "-y", "-v", "error", "-f", "f32le", "-ar", str(frame.sample_rate),
            "-ac", str(frame.channels), "-i", "pipe:0", "-c:a", "mp2",
            "-b:a", str(frame.bitrate), "-f", "mp2", "pipe:1",
        ],
        input=mixed.tobytes(),
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        check=False,
    )
    if encode.returncode:
        raise ValueError(f"ffmpeg could not encode MP2: {encode.stderr.decode(errors='replace').strip()}")
    return encode.stdout


def wrap_crossfade_loop(segment: bytes, frame: Frame, seconds: float) -> bytes:
    """Make one periodic PCM loop whose wrap point is crossfaded and re-encode it."""
    if not shutil.which("ffmpeg"):
        raise ValueError("--wrap-crossfade needs ffmpeg in PATH")

    decode = subprocess.run(
        ["ffmpeg", "-v", "error", "-f", "mp3", "-i", "pipe:0", "-f", "f32le", "-acodec", "pcm_f32le", "pipe:1"],
        input=segment, stdout=subprocess.PIPE, stderr=subprocess.PIPE, check=False,
    )
    if decode.returncode:
        raise ValueError(f"ffmpeg could not decode MP2: {decode.stderr.decode(errors='replace').strip()}")
    samples = array.array("f")
    samples.frombytes(decode.stdout)
    if sys.byteorder != "little":
        samples.byteswap()
    if len(samples) % frame.channels:
        raise ValueError("ffmpeg returned incomplete PCM samples")

    requested_fade_frames = round(seconds * frame.sample_rate)
    # A periodic MP2 stream must end on an MP2 frame boundary.  Otherwise the
    # encoder pads its final partial frame, and that padding becomes audible at
    # the next hardware wrap.  The source range itself is frame-aligned.
    fade_frames = round(requested_fade_frames / 1152) * 1152
    source_frames = len(samples) // frame.channels
    if fade_frames < 1 or fade_frames * 2 >= source_frames:
        raise ValueError(
            f"wrap crossfade must be at least one sample and shorter than half the "
            f"selected segment ({source_frames / frame.sample_rate:.3f}s)"
        )

    fade_values = fade_frames * frame.channels

    # First build the same two-copy crossfade that --crossfade uses.  It is a
    # continuous PCM sequence: A[0:L-F] + fade(A[L-F:L], A[0:F]) + A[F:L].
    # Encoding that sequence lets the MP2 encoder carry its filterbank state
    # across the fade.  We then rotate one frame-aligned period out of it.
    mixed = array.array("f", samples)
    join_start = len(mixed) - fade_values
    for sample_frame in range(fade_frames):
        phase = (sample_frame + 1) / fade_frames * math.pi / 2
        tail_gain, head_gain = math.cos(phase), math.sin(phase)
        base = sample_frame * frame.channels
        for channel in range(frame.channels):
            mixed[join_start + base + channel] = (
                mixed[join_start + base + channel] * tail_gain
                + samples[base + channel] * head_gain
            )
    mixed.extend(samples[fade_values:])

    source_frame_count = source_frames // 1152
    period_frame_count = source_frame_count - fade_frames // 1152
    if source_frame_count * 1152 != source_frames:
        raise ValueError("internal error: wrap loop is not MP2-frame aligned")
    if sys.byteorder != "little":
        mixed.byteswap()
    encode = subprocess.run(
        [
            "ffmpeg", "-y", "-v", "error", "-f", "f32le", "-ar", str(frame.sample_rate),
            "-ac", str(frame.channels), "-i", "pipe:0", "-c:a", "mp2",
            "-b:a", str(frame.bitrate), "-f", "mp2", "pipe:1",
        ],
        input=mixed.tobytes(), stdout=subprocess.PIPE, stderr=subprocess.PIPE, check=False,
    )
    if encode.returncode:
        raise ValueError(f"ffmpeg could not encode MP2: {encode.stderr.decode(errors='replace').strip()}")
    encoded_frames = index_frames(encode.stdout)
    if len(encoded_frames) != source_frame_count * 2 - fade_frames // 1152:
        raise ValueError("ffmpeg did not preserve the expected number of MP2 frames")

    def frame_range(start: int, end: int) -> bytes:
        first = encoded_frames[start]
        last = encoded_frames[end - 1]
        return encode.stdout[first.offset : last.offset + last.size]

    # After the original fade lie A[F:L]; retain its body A[F:L-F], then
    # append the encoded fade.  Repeating this output recreates the original
    # continuous order A[F:L-F] -> fade -> A[F:L-F].
    body_start = source_frame_count
    body_end = source_frame_count + period_frame_count - fade_frames // 1152
    fade_start = source_frame_count - fade_frames // 1152
    fade_end = source_frame_count
    return frame_range(body_start, body_end) + frame_range(fade_start, fade_end)


def estimate_wrap_crossfade(segment: bytes, frame: Frame, maximum_seconds: float) -> tuple[float, float]:
    """Choose the frame-aligned end/start overlap with highest cosine similarity."""
    decode = subprocess.run(
        ["ffmpeg", "-v", "error", "-f", "mp3", "-i", "pipe:0", "-f", "f32le", "-acodec", "pcm_f32le", "pipe:1"],
        input=segment, stdout=subprocess.PIPE, stderr=subprocess.PIPE, check=False,
    )
    if decode.returncode:
        raise ValueError(f"ffmpeg could not decode MP2: {decode.stderr.decode(errors='replace').strip()}")
    samples = array.array("f")
    samples.frombytes(decode.stdout)
    if sys.byteorder != "little":
        samples.byteswap()
    if len(samples) % frame.channels:
        raise ValueError("ffmpeg returned incomplete PCM samples")

    source_frames = len(samples) // frame.channels
    max_overlap_frames = min(
        int(maximum_seconds * frame.sample_rate) // 1152,
        (source_frames // 1152 - 1) // 2,
    )
    if max_overlap_frames < 1:
        raise ValueError("selected segment is too short for the requested automatic crossfade")

    best_frames, best_score = 0, -float("inf")
    for overlap_frames in range(1, max_overlap_frames + 1):
        count = overlap_frames * 1152 * frame.channels
        head = samples[:count]
        tail = samples[-count:]
        dot = sum(left * right for left, right in zip(head, tail))
        head_energy = sum(value * value for value in head)
        tail_energy = sum(value * value for value in tail)
        if head_energy and tail_energy:
            score = dot / math.sqrt(head_energy * tail_energy)
            if score > best_score:
                best_frames, best_score = overlap_frames, score
    if not best_frames:
        raise ValueError("cannot estimate a crossfade from silent audio")
    return best_frames * 1152 / frame.sample_rate, best_score


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("input", type=Path)
    parser.add_argument("output", type=Path)
    parser.add_argument("--start", type=int, default=0, help="first frame (zero-based)")
    parser.add_argument("--end", type=int, help="first frame not included (default: final frame)")
    parser.add_argument("--loops", type=int, default=1, help="number of copies to write")
    parser.add_argument("--start-time", type=float, help="alternative to --start, in seconds")
    parser.add_argument("--duration", type=float, help="with --start-time, range duration in seconds")
    crossfade_group = parser.add_mutually_exclusive_group()
    crossfade_group.add_argument(
        "--crossfade", type=float, metavar="SECONDS",
        help="crossfade every loop join (requires ffmpeg; re-encodes output)",
    )
    crossfade_group.add_argument(
        "--wrap-crossfade", type=float, metavar="SECONDS",
        help="make one periodic loop with a crossfaded wrap point (requires ffmpeg)",
    )
    crossfade_group.add_argument(
        "--auto-crossfade", type=float, nargs="?", const=0.25, metavar="MAX_SECONDS",
        help="automatically choose a periodic wrap crossfade (default search limit: 0.25 s)",
    )
    parser.add_argument(
        "--play", action="store_true",
        help="concatenate and play the written output repeatedly for preview",
    )
    parser.add_argument(
        "--play-loops", type=int, default=32, metavar="COUNT",
        help="number of output-file copies used by --play (default: 32)",
    )
    args = parser.parse_args()

    if args.loops < 1:
        parser.error("--loops must be at least 1")
    if args.start_time is not None and args.start != 0:
        parser.error("use either --start or --start-time")
    if args.duration is not None and args.start_time is None:
        parser.error("--duration requires --start-time")
    if args.duration is not None and args.duration <= 0:
        parser.error("--duration must be positive")
    if args.crossfade is not None and args.crossfade <= 0:
        parser.error("--crossfade must be positive")
    if args.crossfade is not None and args.loops < 2:
        parser.error("--crossfade requires --loops of at least 2")
    if args.wrap_crossfade is not None and args.wrap_crossfade <= 0:
        parser.error("--wrap-crossfade must be positive")
    if args.wrap_crossfade is not None and args.loops != 1:
        parser.error("--wrap-crossfade produces exactly one loop; omit --loops")
    if args.auto_crossfade is not None and args.auto_crossfade <= 0:
        parser.error("--auto-crossfade maximum must be positive")
    if args.auto_crossfade is not None and args.loops != 1:
        parser.error("--auto-crossfade produces exactly one loop; omit --loops")
    if args.play_loops < 2:
        parser.error("--play-loops must be at least 2")

    try:
        data = args.input.read_bytes()
        frames = index_frames(data)
    except (OSError, ValueError) as error:
        print(f"mp2_loop: {error}", file=sys.stderr)
        return 2

    sample_rate = frames[0].sample_rate
    frame_seconds = 1152 / sample_rate
    start = round(args.start_time / frame_seconds) if args.start_time is not None else args.start
    end = (start + round(args.duration / frame_seconds) if args.duration is not None
           else (args.end if args.end is not None else len(frames)))
    if start < 0 or end <= start or end > len(frames):
        parser.error(f"range must satisfy 0 <= start < end <= {len(frames)}")

    first, last = frames[start], frames[end - 1]
    segment = data[first.offset : last.offset + last.size]
    effective_wrap_crossfade = None
    try:
        auto_score = None
        if args.crossfade is not None:
            output = crossfade_loop(segment, first, args.loops, args.crossfade)
        elif args.wrap_crossfade is not None:
            effective_wrap_crossfade = round(round(args.wrap_crossfade * sample_rate) / 1152) * 1152 / sample_rate
            output = wrap_crossfade_loop(segment, first, effective_wrap_crossfade)
        elif args.auto_crossfade is not None:
            effective_wrap_crossfade, auto_score = estimate_wrap_crossfade(segment, first, args.auto_crossfade)
            output = wrap_crossfade_loop(segment, first, effective_wrap_crossfade)
        else:
            effective_wrap_crossfade = None
            output = segment * args.loops
        args.output.write_bytes(output)
    except (OSError, ValueError) as error:
        print(f"mp2_loop: {error}", file=sys.stderr)
        return 2
    print(
        f"wrote frames {start}:{end} ({end - start} frames, "
        f"{(end - start) * frame_seconds:.3f}s) x {args.loops}"
        f"{' with %.3fs crossfades' % args.crossfade if args.crossfade is not None else ''}"
        f"{' with a %.3fs wrap crossfade' % effective_wrap_crossfade if effective_wrap_crossfade is not None else ''} "
        f"{'(auto-correlation %.3f) ' % auto_score if auto_score is not None else ''}"
        f"to {args.output}",
        file=sys.stderr,
    )
    if args.play:
        players = (
            ("ffplay", ["ffplay", "-nodisp", "-autoexit", "-loglevel", "error"]),
            ("vlc", ["vlc", "--play-and-exit"]),
        )
        with tempfile.TemporaryDirectory(prefix="mp2_loop_preview_") as directory:
            preview = Path(directory) / args.output.name
            # Play a physically concatenated elementary stream.  Asking a
            # player to loop a file reinitializes its MP2 decoder at the join.
            preview.write_bytes(args.output.read_bytes() * args.play_loops)
            for name, command in players:
                if shutil.which(name):
                    try:
                        subprocess.run([*command, str(preview)], check=False)
                    except OSError as error:
                        print(f"mp2_loop: could not start {name}: {error}", file=sys.stderr)
                        return 2
                    break
            else:
                print("mp2_loop: --play needs ffplay or vlc in PATH", file=sys.stderr)
                return 2
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
