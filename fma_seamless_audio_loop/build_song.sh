
#!/usr/bin/env bash
set -euo pipefail

# Build the individual song parts.  The runtime sequence is sector-addressed.
ffmpeg -y -i map.flac -b:a 192k -ar 44100 green.mp2
# The intro and restart are complementary ranges of the same original part.
python3 mp2_loop.py green.mp2 start.mp2 --start 0 --end 3
python3 mp2_loop.py green.mp2 restart.mp2 --start 3 --end 39
python3 mp2_loop.py green.mp2 loop1.mp2 --start 36 --end 76 --wrap-crossfade 0.08
python3 mp2_loop.py green.mp2 loop2.mp2 --start 220 --end 260 --wrap-crossfade 0.08

# Keep each part independent; generate_song_sequence.py joins their sectors.
python3 embed_mpeg.py start.mp2 src/sfx_start.c --name start_mpg --guard-pack
python3 embed_mpeg.py restart.mp2 src/sfx_restart.c --name restart_mpg --guard-pack
python3 embed_mpeg.py loop1.mp2 src/sfx1.c --name loop1_mpg
python3 embed_mpeg.py loop2.mp2 src/sfx2.c --name loop2_mpg
python3 generate_song_sequence.py
