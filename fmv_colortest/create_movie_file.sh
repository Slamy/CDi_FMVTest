#!/bin/bash

set -e


ffmpeg -y -i lost_ride_i_frame2.m1v -c copy \
    -f vcd -muxpreload 0.1 -packetsize 2324 \
    cross.mpg

ffmpeg -i cross.mpg out/%d.png

xxd -i cross.mpg  > src/cross_mpg.h
