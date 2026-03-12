#!/bin/bash

set -e

rm -f cross_audio.mpg
rm -f cross_video.mpg

ffmpeg -y -i frames/%02d.png \
    -r 25 -pix_fmt yuv420p -f yuv4mpegpipe - |
     mpeg2enc -v 0 -f 1 -n p -K tmpgenc -r 32 -4 1 -q 6 -b 1150 -o "temp_video.m1v" 

ffmpeg -y -i temp_video.m1v \
    -packetsize 2324 -muxpreload 0.44 -codec:v copy -an cross_video.mpg

# 2 Sectors 11 + 5

ffmpeg -y -i clip.mpg \
    -packetsize 2304 -muxpreload 0.30 \
    -ar 44100 -ac 1 \
    -codec:a mp2 -b:a 32k \
    -vn \
    cross_audio.mpg

xxd -i cross_audio.mpg  > src/cross_audio.h
xxd -i cross_video.mpg  > src/cross_video.h

echo Finished