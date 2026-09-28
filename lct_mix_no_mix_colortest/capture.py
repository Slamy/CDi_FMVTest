import cv2
import subprocess

video_device = "/dev/v4l/by-id/usb-fushicai_usbtv007_300000000002-video-index0"
PAL_WIDTH = 720
PAL_HEIGHT = 576


def select_pal_standard():
    """Select PAL at the V4L2 device, before OpenCV opens the stream."""
    command = ["v4l2-ctl", "--device", video_device]
    try:
        subprocess.run(command + ["--set-standard=PAL"], check=True, capture_output=True, text=True)
        standard = subprocess.run(command + ["--get-standard"], check=True, capture_output=True, text=True)
    except FileNotFoundError:
        raise RuntimeError("v4l2-ctl is required to select the PAL input standard")
    except subprocess.CalledProcessError as error:
        raise RuntimeError("could not select PAL on {}: {}".format(video_device, error.stderr.strip()))

    if "PAL" not in standard.stdout.upper():
        raise RuntimeError("capture device did not enter PAL mode: {}".format(standard.stdout.strip()))

    print("Capture standard: {}".format(standard.stdout.strip()))


def capture_still_frame():
    select_pal_standard()
    cap = cv2.VideoCapture(video_device)

    # Check if camera was opened correctly
    if not (cap.isOpened()):
        raise RuntimeError("Could not open video device")

    cap.set(cv2.CAP_PROP_FRAME_WIDTH, PAL_WIDTH)
    cap.set(cv2.CAP_PROP_FRAME_HEIGHT, PAL_HEIGHT)

    # For some reason the brightness can only be set after reading one frame
    ret, frame = cap.read()
    if not ret:
        cap.release()
        raise RuntimeError("Could not read the first PAL frame")
    #print(cap.get(cv2.CAP_PROP_BRIGHTNESS))
    #print(cap.get(cv2.CAP_PROP_CONTRAST))
    cap.set(cv2.CAP_PROP_BRIGHTNESS, 588)
    cap.set(cv2.CAP_PROP_CONTRAST,304)

    for i in range(30):
        ret, frame = cap.read()
        if not ret:
            cap.release()
            raise RuntimeError("Could not read a settled PAL frame")

    h, w = frame.shape[:2]
    if (w, h) != (PAL_WIDTH, PAL_HEIGHT):
        cap.release()
        raise RuntimeError("expected PAL capture {}x{}, got {}x{}".format(PAL_WIDTH, PAL_HEIGHT, w, h))

    print("Captured PAL frame: {}x{}".format(w, h))

    cap.release()
    return frame

image = capture_still_frame()
cv2.imwrite("cap.png", image)

# v4l2-ctl -d /dev/video0 --list-ctrls
# v4l2-ctl -d /dev/video0 --get-ctrl=brightness,contrast


