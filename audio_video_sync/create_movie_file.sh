#!/bin/bash

set -e

# From YouTube
# Audio Video Sync Test Card - 16:9 1080p 30fps

if [ -f 'Audio Video Sync Test Card - 16_9 1080p 30fps [QzomK1fdSUg].mkv' ]; then
    echo "Movie already downloaded..."
else
    echo "Video missing. Downloading from YouTube"
    yt-dlp https://youtu.be/QzomK1fdSUg
fi

ffmpeg -y -t 120 -i 'Audio Video Sync Test Card - 16_9 1080p 30fps [QzomK1fdSUg].mkv' \
    -r 30 -s 384:256 -pix_fmt yuv420p -f yuv4mpegpipe - |
     mpeg2enc -v 0 -f 1 -n p -K tmpgenc -r 32 -4 1 -q 6 -b 1150 -o "green.m1v" 

ffmpeg -y -t 120 -i 'Audio Video Sync Test Card - 16_9 1080p 30fps [QzomK1fdSUg].mkv' \
    -b:a 224k -ar 44100 green.mp2

echo Finished