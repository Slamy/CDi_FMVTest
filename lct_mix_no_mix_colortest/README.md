# Plane A Color Test

This is a Plane A-only DYUV color test. There is no MPEG functionality involved
here.

Plane A is configured as DYUV and fills the screen with ten neutral-chroma
grayscale bars, targeting luma values 0, 16, 32, 64, 128, 192, 223, 235, 239,
and 255. (The closest representable DYUV delta is used at each bar edge.)
Plane A fills the entire screen.

Example taken via video grabber:

![Video grabber picture of this example](example.png)

Use [fmv_colortest](../fmv_colortest/) to tune the USB video grabber to ensure all levels are visible.
