# FMV Inverted Flying Mattes

Does the opposite of [Technical Note #103](http://icdia.co.uk/notes/technote103.pdf) and keeps a moving
part of a video always at the same position.

The used frames look like this:

![White box top left](frames/0.png)
![White box top right](frames/1.png)
![White box bottom right](frames/2.png)
![White box bottom left](frames/3.png)

The goal of the application is to always have the white square at the same position using `mv_org()`.

This is a replication of a technique used by "The Lost Ride" to pan the video to aim your gun.
The result should look like a buzz saw on the screen.

It is expected that the start of the loop is problematic.

## Preparing MPEG file

Execute `./create_movie_file.sh`, which converts the PNG files into an MPEG file.
