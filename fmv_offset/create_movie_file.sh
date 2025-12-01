#!/bin/bash

set -e

ffmpeg -y -i parrot.png \
    -f vcd -muxrate 1411200 -muxpreload 0.44 -packetsize 2324 \
    -s 384x256 -r 25 \
    -codec:v mpeg1video -g 15 -b:v 1150k -maxrate:v 1150k -minrate:v 1150k -bufsize:v 327680 \
    cross.mpg

xxd -i cross.mpg  > src/cross_mpg.h
