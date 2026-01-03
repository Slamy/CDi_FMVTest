#!/bin/bash

set -e

rm -f *.mpg

ffmpeg -y -i mono_sine.wav \
    -packetsize 2304 -muxpreload 0.44 \
    -ar 44100 -ac 1 \
    -codec:a mp2 -b:a 192k \
    -vn \
    stereo_sine.mpg

xxd -i stereo_sine.mpg  > src/stereo_sine_mpg.h

echo Finished