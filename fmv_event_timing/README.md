# FMV Event timing

Plays a short video loop and draws on top of the frames

The used frames look like this:

![White box top left](frames/0.png)
![White box top right](frames/1.png)
![White box bottom right](frames/2.png)
![White box bottom left](frames/3.png)

The goal of the application is to draw slightly outside the video and also inside the video
on top of the white squares. If everything is in sync, you should only see 4 blinking dots
outside the video.

If the PIC events are not in sync, 4 additional blinking dots can be seen.

In case of real analog VMPEG hardware, keep in mind that switching from base case video to DVC overlay takes time and
causes a very small column of "dirt" to appear. It is narrower than a pixel, but is visible in the white squares regardless.

This application also counts the PIC events. When LPD occurs, it will print the number.
In case all frames are shown, 164 is the correct number.

## Preparing MPEG file

Execute `./create_movie_file.sh`, which converts the PNG files into an MPEG file.
