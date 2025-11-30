# FMVTest

A collection of example applications to utilize the MPEG hardware of the Philips CD-i

The applications are designed to be played from disc and loaded via serial stub

[Playing Big Buck Bunny](big_buck_bunny/)
[Offset between FMV and Base case](fmv_offset)

## Compiling under Linux

### Prerequisites

Clone https://github.com/TwBurn/cdi-sdk by updating the git submodules and have it mounted as D: drive in winecfg. That can be done via script.

	ln -s $(realpath cdi-sdk) ~/.wine/dosdevices/d:

For stub loading this application on a CD-i using the serial port, [cdilink](https://www.cdiemu.org/?body=site/cdilink.htm) is required.

### Compiling

Go to one of the examples and do this

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

	wine wcdiemu-v053b9.exe disk/FMVTEST.CDI -playcdi -start

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
