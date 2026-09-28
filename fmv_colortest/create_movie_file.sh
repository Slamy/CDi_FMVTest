#!/bin/bash

set -e

cp reference.png 0.png
cp reference.png 1.png
cp reference.png 2.png
cp reference.png 3.png

ffmpeg -y -i %d.png \
    -f vcd -muxpreload 0.1 -packetsize 2324 \
    -color_range pc \
    -s 384x256 -r 25 \
    -codec:v mpeg1video -g 15 -b:v 1150k -maxrate:v 1150k -minrate:v 1150k -bufsize:v 327680 \
    cross.mpg

ffmpeg -i cross.mpg out/%d.png

xxd -i cross.mpg  > src/cross_mpg.h
