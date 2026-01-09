# FMV Audio & Video Sync

Plays a very short MPEG file on repeat.
It contains a small white block for 5 frames and a sine wave at the same time.

This example can be used to check the synchronization between MPEG audio and MPEG video.
Use your soundcard and connect the CVBS signal and one of the audio channels.
The upper channel is the audio, the lower is the CVBS.
The vertical synchronization is visible, as well as the square that goes from the top to the bottom

![Recoding of Audio and Video using sound hardware](example_recording.png)

It should be noted that for unknown reasons, there are 9 frames with white pixel data.
Since 5 frames contain a white square, I would have expected 10 frames...

