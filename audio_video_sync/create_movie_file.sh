#!/bin/bash

set -e

ffmpeg -stream_loop 800 -y -i frames/%d.png \
    -r 25 -pix_fmt yuv420p -f yuv4mpegpipe - |
     mpeg2enc -v 0 -f 1 -n p -K tmpgenc -r 32 -4 1 -q 6 -b 1150 -o "green.m1v"

echo Finished
