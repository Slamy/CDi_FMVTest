#!/bin/bash

set -e

rm -f cross_audio.mpg
rm -f cross_video.mpg

ffmpeg -y -i clip.mpg \
    -packetsize 2324 -muxpreload 0.44 \
    -s 16x128 -r 25 \
    -codec:v mpeg1video -g 15 -b:v 1150k -maxrate:v 1150k -bufsize:v 327680 \
	-an \
    cross_video.mpg

ffmpeg -y -i clip.mpg \
    -packetsize 2304 -muxpreload 0.44 \
    -ar 44100 -ac 1 \
    -codec:a mp2 -b:a 32k \
    -vn \
    cross_audio.mpg

xxd -i cross_audio.mpg  > src/cross_audio.h
xxd -i cross_video.mpg  > src/cross_video.h

echo Finished