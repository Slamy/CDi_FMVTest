#!/bin/bash

set -e

ffmpeg -stream_loop 18 -y -i frames/%d.png \
    -packetsize 2324 -muxpreload 0.44 \
    -s 32x32 -r 25 \
    -codec:v mpeg1video -g 15 -b:v 1150k -maxrate:v 1150k -bufsize:v 327680 \
	-an \
    clip.mpg

xxd -i clip.mpg  > src/clip.h

echo Finished
