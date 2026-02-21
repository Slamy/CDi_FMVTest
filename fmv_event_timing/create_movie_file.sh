#!/bin/bash

set -e

ffmpeg -stream_loop 40 -y -i frames/%d.png \
    -r 25 -pix_fmt yuv420p -f yuv4mpegpipe - |
     mpeg2enc -v 0 -f 1 -n p -K tmpgenc -r 32 -4 1 -q 6 -b 1150 -o "temp_video.m1v" 

ffmpeg -y -i temp_video.m1v \
    -packetsize 2324 -muxpreload 0.44 -codec:v copy -an clip.mpg

xxd -i clip.mpg  > src/clip.h

echo Finished
