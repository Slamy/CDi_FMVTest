#!/bin/bash

set -e

rm -f cross_video.mpg

ffmpeg -y -stream_loop 20 -r 25 -start_number 0 -i pic%d.png \
    -packetsize 2324 -muxpreload 0.44 \
    -s 16x200 -r 25 \
    -codec:v mpeg1video -g 15 -b:v 1150k -maxrate:v 1150k -bufsize:v 327680 \
	-an \
    cross_video.mpg

xxd -i cross_video.mpg  > src/cross_video.h

echo Finished

# To reverse ffmpeg -i ../cross_video.mpg p%02d.png
