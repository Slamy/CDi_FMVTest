#!/usr/bin/env python3
"""Automatically tune the USB grabber from the direct-DVC black/white bars.

The fmv_colortest pattern must be visible. This program samples the full-height
black and white bars, then uses finite-difference measurements of brightness
and contrast to bring them to digital video black/white (16 and 235).
"""

import argparse

import cv2


DEFAULT_DEVICE = "/dev/v4l/by-id/usb-fushicai_usbtv007_300000000002-video-index0"


def clamp(value: int, minimum: int, maximum: int) -> int:
    return max(minimum, min(maximum, value))


def read_settled_frame(cap: cv2.VideoCapture, settle_frames: int):
    """Discard frames after a control change and return the last one."""
    frame = None
    for _ in range(settle_frames):
        ok, frame = cap.read()
        if not ok:
            raise RuntimeError("Could not read a frame from the video device")
    return frame


def measure_levels(frame, bar_width: int) -> tuple[float, float]:
    """Measure the first and last bars over the complete captured picture."""
    width = frame.shape[1]
    black_x = width // 20
    white_x = width * 19 // 20
    half = bar_width // 2
    gray = cv2.cvtColor(frame, cv2.COLOR_BGR2GRAY)

    def bar_mean(x: int) -> float:
        return float(gray[:, x - half : x + half].mean())

    return bar_mean(black_x), bar_mean(white_x)


def set_controls(cap: cv2.VideoCapture, brightness: int, contrast: int) -> None:
    cap.set(cv2.CAP_PROP_BRIGHTNESS, brightness)
    cap.set(cv2.CAP_PROP_CONTRAST, contrast)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--device", default=DEFAULT_DEVICE, help="V4L2 device path")
    parser.add_argument("--target-black", type=float, default=16, help="target black luma (default: 16)")
    parser.add_argument("--target-white", type=float, default=235, help="target white luma (default: 235)")
    parser.add_argument("--iterations", type=int, default=8, help="maximum adjustment iterations (default: 8)")
    parser.add_argument("--probe", type=int, default=8, help="control change used to measure the local gradient (default: 8)")
    parser.add_argument(
        "--settle-frames",
        type=int,
        default=30,
        help="frames to discard after each adjustment (default: 30)",
    )
    parser.add_argument("--max-step", type=int, default=64, help="largest single control adjustment (default: 64)")
    parser.add_argument("--min-value", type=int, default=0, help="minimum control value (default: 0)")
    parser.add_argument("--max-value", type=int, default=1023, help="maximum control value (default: 1023)")
    parser.add_argument(
        "--bar-width", type=int, default=32, help="width sampled from each full-height bar in pixels (default: 32)"
    )
    parser.add_argument("--output", default="auto-cap.png", help="final frame filename (default: auto-cap.png)")
    args = parser.parse_args()

    cap = cv2.VideoCapture(args.device)
    if not cap.isOpened():
        raise SystemExit(f"Could not open video device: {args.device}")

    try:
        # The grabber only accepts control changes after a captured frame.
        read_settled_frame(cap, args.settle_frames)

        brightness = 639
        contrast = 285

        for iteration in range(1, args.iterations + 1):
            set_controls(cap, brightness, contrast)
            black, white = measure_levels(read_settled_frame(cap, args.settle_frames), args.bar_width)
            black_error = args.target_black - black
            white_error = args.target_white - white
            print(
                f"{iteration}: brightness={brightness}, contrast={contrast}; "
                f"black={black:.1f} ({black_error:+.1f}), white={white:.1f} ({white_error:+.1f})"
            )
            if max(abs(black_error), abs(white_error)) <= 1:
                break

            # Some grabbers apply controls lazily or have coarse steps.  Increase
            # the probe until both controls produce a measurable response.
            probe = args.probe
            while True:
                brightness_probe = clamp(brightness + probe, args.min_value, args.max_value)
                contrast_probe = clamp(contrast + probe, args.min_value, args.max_value)
                if brightness_probe == brightness or contrast_probe == contrast:
                    raise RuntimeError("A control is at its configured limit; adjust --min-value/--max-value")

                set_controls(cap, brightness_probe, contrast)
                black_brightness, white_brightness = measure_levels(
                    read_settled_frame(cap, args.settle_frames), args.bar_width
                )
                set_controls(cap, brightness, contrast_probe)
                black_contrast, white_contrast = measure_levels(
                    read_settled_frame(cap, args.settle_frames), args.bar_width
                )

                # Jacobian: measured black/white change per control unit.
                db_db = (black_brightness - black) / (brightness_probe - brightness)
                dw_db = (white_brightness - white) / (brightness_probe - brightness)
                db_dc = (black_contrast - black) / (contrast_probe - contrast)
                dw_dc = (white_contrast - white) / (contrast_probe - contrast)
                determinant = db_db * dw_dc - db_dc * dw_db
                if abs(determinant) >= 0.001:
                    break
                if probe >= args.max_step:
                    raise RuntimeError(
                        "Grabber controls did not produce independent measurable changes; "
                        "try a larger --settle-frames or verify the device controls"
                    )
                probe = min(probe * 2, args.max_step)
                print(f"No measurable response yet; retrying with probe={probe}")

            # Solve J * adjustment = target - measurement.
            brightness_change = (black_error * dw_dc - db_dc * white_error) / determinant
            contrast_change = (db_db * white_error - black_error * dw_db) / determinant
            brightness += round(max(-args.max_step, min(args.max_step, brightness_change)))
            contrast += round(max(-args.max_step, min(args.max_step, contrast_change)))
            brightness = clamp(brightness, args.min_value, args.max_value)
            contrast = clamp(contrast, args.min_value, args.max_value)

        set_controls(cap, brightness, contrast)
        frame = read_settled_frame(cap, args.settle_frames)
        black, white = measure_levels(frame, args.bar_width)
        cv2.imwrite(args.output, frame)
        print(
            f"Final: brightness={brightness}, contrast={contrast}; "
            f"black={black:.1f}, white={white:.1f}; wrote {args.output}"
        )
    finally:
        cap.release()


if __name__ == "__main__":
    main()
