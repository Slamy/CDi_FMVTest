# FMVTest

A collection of example applications to utilize the MPEG hardware of the Philips CD-i

The applications are designed to be played from disc and loaded via serial stub

* [Playing Big Buck Bunny](big_buck_bunny/)
* [Offset between FMV and Base case](fmv_offset/)
* [Offset between FMV and Base case in VCD Mode](fmv_offset_vcd/)
* [Moving cropped window](fmv_moving_window/)
* [Moving cropped window with timing analysis](fmv_moving_window_joystick_dca/)
* [Moving cropped window with state analysis](fmv_moving_window_state_analysis/)
* [Playback of music from Lost Eden disk](lost_eden_music/)
* [Phase relation between audio and video](audio_video_sync/)
* [Attenuation of MPEG Audio](audio_attenuation/)
* [Attenuation of MPEG Audio compared to the CDIC](audio_attenuation_vs_cdic/)
* [Replication of Lost Ride railroad ambience](lost_ride_hostplay/)
* [Failed seamless MPEG audio loop experiment](lost_ride_seamless_experiment//)
* [Inverted flying mattes](fmv_flying_mattes/)

The intention of this project is to be helpful for emulator developers as well,
since the source code can be augmented with debugging prints.

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

Not feeling like burning yet another CD today? All examples here can be launched via stub loading over UART!

	./stub_load.sh

Not only that, you don't need to touch the CD-i for these tests. All examples here will reboot the the machine into the stub loader when another program shall be started!

### Copy to MiSTer

Keep in mind that the MiSTer CD-i core is not yet fully compatible with DVC software. It won't work!

	scp disk/FMVTEST.CUE disk/FMVTEST.BIN root@mister:/media/fat/games/CD-i

## FMVTest by Jeffrey Janssen - nobelia@nmotion.nl

I would have never been able to write this code since the documentation is scarce.
The original source code was made by https://github.com/TwBurn, I only have modified it and got allowance of GitHub upload.

Thanks also go out to https://github.com/cdifan as he helped me in getting an understanding on the DVC hardware and software API
