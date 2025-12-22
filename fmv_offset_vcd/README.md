# FMV Offset Test (VCD in tray)

Loads a MPEG stream with a frame into memory and shows it using the DVC.
Draws some pixels in the base case video to show the alignment between both video outputs.

This time, there is a twist. There is a VCD in the tray (which must be spinning!).
Using the VCD module, the OS will configure the DVC into VCD pixel clock mode.
The vertical raster is unchanged by this but the horizontal one is stretched.

The base image is this:

![White squares with crosses](cross_256.png)

On the CD-i it should look like this:

![Illustration on CDI 210/05](example.png)

Note how the horizontal resolution is not capable of displaying the whole 352 pixel of a VCD.
Also note that this example will only work via stub loading!
The process of detecting a VCD is not yet known to me.

It is also interesting that the first column of the MPEG footage is not visible.

## Preparing MPEG file

Execute `./create_movie_file.sh`, which converts the PNG into an MPEG file.
Feel free to replace the PNG file with another image.

## Official documentation

Thanks to CD-i Fan for this hint:

There's an article in [Interactive Engineer Volume 5, No. 5, September/October 1996](http://icdia.co.uk/iengineer/IE_9605.pdf)  called Building VCD compliant disc that might answer some of these questions. It specifically states:

    VCD streams are becoming very popular in
    MediaMogul applications. But because of their
    352x288 pixel format,the pictures appears to be
    horizontally squeezed and have a black border on
    both sides.

    A simple way to correct this problem is to
    build and burn the disc as a white book VCD.
    When this disc is played,the DV decoder switches
    to VCD (white book) playback mode and displays
    the pictures without black borders and in the
    correct aspect ratio.

The article The CSD file and DV cartridges in the same IE issue has more information, in particular it states:

    The way around: the player is White Book capable when the ‘vcd’ module is  detected.
    [...]
    A useful hint when developing titles containing
    MPEG streams: the ‘White Book’cartridges all have a
    Sample Rate Converter (SRC) on board, converting the
    MPEG pixel frequency from 15 Mhz (Green Book) to
    13.5 Mhz. This results in slightly larger backplane
    pixels. A film on videoCD –encoded with a width of
    352 pixels– will not show full screen on a CD-i in
    Green Book mode,while showing full screen on a CD
    i in White Book mode. ‘White Book’ cartridges
    recognise the type of disc inserted into the player and
    switch to the appropriate mode. Older cartridges,
    however, can’t switch between these modes and will
    show the videoCD not full screen and in the wrong
    aspect ratio.

