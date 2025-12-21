# FMV Moving Windows

Loads a MPEG stream with a frame into memory and shows it using the DVC.
Only shows a cropped view of the MPEG footage and moves the cropped view around like a bouncing "DVD logo"

The base image is this:

![Picture of a parrot](parrot.png)

The resolution of 384x256 is the maximum possible vertical resolution when 384 horizontal pixels are required,
due to the 396 macro block limitation of the CD-i.
This image is converted to an MPEG movie and the base case video is used to fill out
the 2x2 center pixels of the crosses.

## Preparing MPEG file

Execute `./create_movie_file.sh`, which converts the PNG into an MPEG file.
Feel free to replace the PNG file with another image.