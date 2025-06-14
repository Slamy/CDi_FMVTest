# FMVTest

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

### Prerequisites

Clone https://github.com/TwBurn/cdi-sdk by updating the git submodules and have it mounted as D: drive in winecfg.

For stub loading this application on a CD-i using the serial port, [cdilink](https://www.cdiemu.org/?body=site/cdilink.htm) is required.

### Compiling

	WINEPATH=D:/DOS/BIN wine D:/dos/bin/bmake.exe link

### Cleanup

	WINEPATH=D:/DOS/BIN wine D:/dos/bin/bmake.exe clean

### Mastering into CDI/TOC file

	dosbox master.bat -exit

The resulting image can be loaded into cdiemu

### Compiling, mastering and CUE/BIN conversion in one step

This approach is crude and might not work on all machines.
It makes use of xdotool to automate button presses.

	./make_image.sh 

### Start image on MAME

Keep in mind that MAME currently has no DVC emulation! It won't work!

	mame cdimono1 -cdrom disk/FMVTEST.CUE

### Start image on cdiemu

	wine wcdiemu-v053b7.exe disk/FMVTEST.CDI -playcdi -start

### Start application via stub loader

Not feeling like burning yet another CD today?

Have `Preview 20250326` of https://twburn.itch.io/skyways in your CD-i before launch.
The contained movie file has the same filename and will be used when loading via serial port.

### Copy to MiSTer

Keep in mind that the MiSTer CD-i core currently has no DVC emulation. It won't work!

	scp disk/FMVTEST.CUE disk/FMVTEST.BIN root@mister:/media/fat/games/CD-i

## FMVTest by Jeffrey Janssen - nobelia@nmotion.nl

I would have never been able to write this code since the documentation is scarce.
The original source code was made by https://github.com/TwBurn, I only have modified it and got allowance of GitHub upload.

Thanks also go out to https://github.com/cdifan as he helped me in getting an understanding on the DVC hardware and software API
