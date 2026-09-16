
#!/usr/bin/env bash
set -euo pipefail

# One periodic MP2 loop: the crossfade sits at the stream's wrap point.
ffmpeg -y -i map.flac -b:a 192k -ar 44100 green.mp2
python3 mp2_loop.py green.mp2 loop1.mp2 --start 36 --end 76 --wrap-crossfade 0.08
python3 mp2_loop.py green.mp2 loop2.mp2 --start 220 --end 260 --wrap-crossfade 0.08

# Embed the two periods separately.  The CD-i player selects loop 1 four
# times, then loop 2 four times, without restarting its MPEG stream.
python3 embed_mpeg.py loop1.mp2 src/sfx1.c --name loop1_mpg
python3 embed_mpeg.py loop2.mp2 src/sfx2.c --name loop2_mpg
