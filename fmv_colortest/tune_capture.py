#!/usr/bin/env python3
"""Live brightness/contrast tuner for the USB video grabber.

Use the left/right arrows for brightness, up/down arrows for contrast, or drag
the sliders.  Press Space to save the current frame, and q or Escape to quit.
"""

import argparse

import cv2


DEFAULT_DEVICE = "/dev/v4l/by-id/usb-fushicai_usbtv007_300000000002-video-index0"
WINDOW_NAME = "CD-i capture tuner"


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--device", default=DEFAULT_DEVICE, help="V4L2 device path")
    parser.add_argument("--max-value", type=int, default=1023, help="maximum slider value (default: 1023)")
    parser.add_argument("--output", default="cap.png", help="file written by Space (default: cap.png)")
    args = parser.parse_args()

    cap = cv2.VideoCapture(args.device)
    if not cap.isOpened():
        raise SystemExit(f"Could not open video device: {args.device}")

    # This grabber only accepts the controls after at least one captured frame.
    ok, frame = cap.read()
    if not ok:
        cap.release()
        raise SystemExit("Could not read a frame from the video device")

    brightness = round(cap.get(cv2.CAP_PROP_BRIGHTNESS))
    contrast = round(cap.get(cv2.CAP_PROP_CONTRAST))
    slider_max = max(args.max_value, brightness, contrast)

    def apply_controls(_: int = 0) -> None:
        cap.set(cv2.CAP_PROP_BRIGHTNESS, cv2.getTrackbarPos("Brightness", WINDOW_NAME))
        cap.set(cv2.CAP_PROP_CONTRAST, cv2.getTrackbarPos("Contrast", WINDOW_NAME))

    cv2.namedWindow(WINDOW_NAME, cv2.WINDOW_NORMAL)
    cv2.createTrackbar("Brightness", WINDOW_NAME, brightness, slider_max, apply_controls)
    cv2.createTrackbar("Contrast", WINDOW_NAME, contrast, slider_max, apply_controls)
    apply_controls()

    print("Left/Right: brightness  Up/Down: contrast  Space: save frame  q/Esc: quit")
    while True:
        ok, frame = cap.read()
        if not ok:
            print("Frame read failed")
            break

        requested_brightness = cv2.getTrackbarPos("Brightness", WINDOW_NAME)
        requested_contrast = cv2.getTrackbarPos("Contrast", WINDOW_NAME)
        active_brightness = cap.get(cv2.CAP_PROP_BRIGHTNESS)
        active_contrast = cap.get(cv2.CAP_PROP_CONTRAST)
        cv2.putText(
            frame,
            f"brightness {requested_brightness} ({active_brightness:.0f})  "
            f"contrast {requested_contrast} ({active_contrast:.0f})",
            (12, 28),
            cv2.FONT_HERSHEY_SIMPLEX,
            0.65,
            (0, 255, 0),
            2,
            cv2.LINE_AA,
        )
        cv2.imshow(WINDOW_NAME, frame)

        key = cv2.waitKey(1) & 0xFF
        if key in (ord("q"), 27):
            break
        if key == ord(" "):
            cv2.imwrite(args.output, frame)
            print(f"Wrote {args.output}")
        elif key in (81, 83):  # left/right
            step = -1 if key == 81 else 1
            cv2.setTrackbarPos("Brightness", WINDOW_NAME, max(0, min(slider_max, requested_brightness + step)))
        elif key in (82, 84):  # up/down
            step = 1 if key == 82 else -1
            cv2.setTrackbarPos("Contrast", WINDOW_NAME, max(0, min(slider_max, requested_contrast + step)))

    cap.release()
    cv2.destroyAllWindows()


if __name__ == "__main__":
    main()
