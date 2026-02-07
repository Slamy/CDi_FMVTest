# Super Attract Mode VCD bridge

This is a very primitive replacement for the Philips "Video-CD on CD-i" bridge application.
It tries to play "/cd/SPACEACE.RTF" and if that fails, tries to play "/cd/MPEGAV/AVSEQ01.DAT",
which is the standard path for `vcdxbuild`.

The video will be played on repeat.

It offers some analysis tools to confirm that the MPEG decoder is actually in good conditions.
The serial port might print some warnings and INFOs.

    Started Play
    BUFFER UNDERFLOW
    PIC: 704 480 - 32 40
    PICs in FIFO: 1
    PICs in FIFO: 1
    BUFFER UNDERFLOW
    PICs in FIFO: 0
    LAST PIC
    PICs in FIFO: 0
    File has ended
    Play Stopped
    MV SIG
    MA SIG

These messages are available on MiSTer via

    microcom /dev/ttyS1 -s 115200

## Compiling under Linux

Please follow the advice from the root of the project

## Start application via stub loader

Not feeling like burning yet another CD today?

Have a VCD in your CD-i before launch.
The contained movie file has the same filename and will be used when loading via serial port.

## Integration into VCD

An example `videocd.xml` file for usage with `vcdxbuild`:

    <?xml version="1.0"?>
    <!DOCTYPE videocd PUBLIC "-//GNU//DTD VideoCD//EN" "http://www.gnu.org/software/vcdimager/videocd.dtd">
    <videocd xmlns="http://www.gnu.org/software/vcdimager/1.0/" class="vcd" version="2.0">
    <info><album-id/><volume-count>1</volume-count><volume-number>1</volume-number><restriction>0</restriction></info>
    <pvd>
        <volume-id>Arcade-_7UP_Pac-Man_Commercial_1982Cropped</volume-id>
        <system-id>CD-RTOS CD-BRIDGE</system-id>
        <application-id>CDI/MISTRVCD.APP;1</application-id>
    </pvd>
    <filesystem>
        <folder><name>SEGMENT</name></folder>
        <folder>
        <name>CDI</name>
        <file src="MISTRVCD.APP"><name>MISTRVCD.APP</name></file>
        </folder>
    </filesystem>
    <sequence-items>
        <sequence-item src="compliant.mpg" id="sequence-00">
        <default-entry id="entry-000"/>
        </sequence-item>
    </sequence-items>
    </videocd>

