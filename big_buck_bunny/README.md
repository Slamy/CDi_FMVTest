# Big Buck Bunny

Simple MPEG Video and Audio player for the Philips CD-i.

Features
- Provides an example for MPEG Playback
- Prints relevant events on the serial port
- Can be modified to only play Video or Audio
- Might be helpful for Emulator development

## Preparing MPEG file

To avoid any legal issues, this example does not provide a MPEG file.
Instead, it relies on public domain videos, in this case the short film ["Big Buck Bunny"](https://peach.blender.org/download/)

Execute `./create_movie_file.sh` which downloads the movie and transcodes it using `ffmpeg`.

**Note:** For some reason, FFmpeg does not provide optimal video quality when encoding MPEG1 with a bitrate required for VCD and the Philips CD-i. The I frames tend to have certain artifacts.
Since this became a rabbit hole, [another project](https://github.com/Slamy/MPEG1_Handbook) was created to discuss this issue and provide solutions with different MPEG encoders.

Since not video quality, but building a MPEG capable application for the CD-i is the emphasis here, we are accepting the lack of quality for the moment.

## Compiling under Linux

Please follow the advice from the root of the project

## Start application via stub loader

Not feeling like burning yet another CD today?

Have `Preview 20250326` of https://twburn.itch.io/skyways in your CD-i before launch.
The contained movie file has the same filename and will be used when loading via serial port.
