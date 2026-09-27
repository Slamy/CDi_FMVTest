import cv2

video_device = "/dev/v4l/by-id/usb-fushicai_usbtv007_300000000002-video-index0"


def capture_still_frame():
    cap = cv2.VideoCapture(video_device)
    w = cap.get(cv2.CAP_PROP_FRAME_WIDTH)
    h = cap.get(cv2.CAP_PROP_FRAME_HEIGHT)
    print(w, h)

    # Check if camera was opened correctly
    if not (cap.isOpened()):
        print("Could not open video device")

    # For some reason the brightness can only be set after reading one frame
    ret, frame = cap.read()
    #print(cap.get(cv2.CAP_PROP_BRIGHTNESS))
    #print(cap.get(cv2.CAP_PROP_CONTRAST))
    cap.set(cv2.CAP_PROP_BRIGHTNESS, 588)
    cap.set(cv2.CAP_PROP_CONTRAST,304)

    for i in range(30):
        ret, frame = cap.read()

    cap.release()
    return frame

image = capture_still_frame()
cv2.imwrite("cap.png", image)

# v4l2-ctl -d /dev/video0 --list-ctrls
# v4l2-ctl -d /dev/video0 --get-ctrl=brightness,contrast



