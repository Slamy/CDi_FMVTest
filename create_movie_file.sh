#!/bin/bash

set -e

# Download if not yet existing
wget -nc https://freepd.com/music/Arpent.mp3

# Cut out some interesting sounding parts and apply typical green book encoding
ffmpeg -y -i "Arpent.mp3" -ss 00:00:31.80 -t 0.5 -b:a 224k -ar 44100 fma.mpg

xxd -i fma.mpg > src/sfx.c