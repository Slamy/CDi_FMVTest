#!/bin/bash

set -e

if [ -f "big_buck_bunny_1080p_h264.mov" ]; then
    echo "Big Buck Bunny already downloaded..."
else
    echo "Big Buck Bunny missing. Downloading from blender.org"
    wget https://download.blender.org/peach/bigbuckbunny_movies/big_buck_bunny_1080p_h264.mov
fi

# --- Video CD style ---
# Note: FFmpeg struggles at I frames with too much information in the image. It causes periodic artefacts
#ffmpeg -y -i "big_buck_bunny_1080p_h264.mov" -filter:v 'crop=ih/3*4:ih' -target pal-vcd vcd.mpg
#ffmpeg -y -i vcd.mpg -vcodec copy -an vcd.m1v
#ffmpeg -y -i vcd.mpg -acodec copy -vn vcd.mp2

# --- Green Book style ---
# Note: FFmpeg struggles at I frames with too much information in the image. It causes periodic artefacts
ffmpeg -y -i "big_buck_bunny_1080p_h264.mov" \
    -filter_complex '[0:v]crop=ih/3*4:ih[cropped];[cropped]scale=384:256[scaled]' \
    -b:v 1150k -minrate 1150k -maxrate 1150k -bufsize 224k -map "[scaled]" green.m1v
ffmpeg -y -i "big_buck_bunny_1080p_h264.mov" -b:a 224k -ar 44100 green.mp2

# Alternative for better quality

# Prepare a high quality downscaled version for using TMPGEnc as a Encoder
# ffmpeg -y -i "big_buck_bunny_1080p_h264.mov" \
#     -filter_complex '[0:v]crop=ih/3*4:ih[cropped];[cropped]scale=384:256[scaled]' \
#     -map "[scaled]" -q:v 0 scaled_hq.avi
# Afterwars use TMPGEnc with ES [Video only]
