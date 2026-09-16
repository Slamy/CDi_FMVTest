#python3 mp2_loop.py green.mp2 loop.mp2 --start 37 --end 77 --loops 8 --crossfade 0.08 --play
#python3 mp2_loop.py green.mp2 loop.mp2 --start 37 --end 77 --auto-crossfade 0.08

ffmpeg -y -i map.flac -b:a 192k -ar 44100 green.mp2
python3 mp2_loop.py green.mp2 loop1.mp2 --start 36 --end 76 --wrap-crossfade 0.08
python3 mp2_loop.py green.mp2 loop2.mp2 --start 220 --end 260 --wrap-crossfade 0.08

# A test to check the inner loop
cat \
    loop1.mp2 loop1.mp2 loop1.mp2 loop1.mp2 \
    loop2.mp2 loop2.mp2 loop2.mp2 loop2.mp2 \
    loop1.mp2 loop1.mp2 loop1.mp2 loop1.mp2 \
    loop2.mp2 loop2.mp2 loop2.mp2 loop2.mp2 \
    > test1.mp2

# A test for the outer loop
cat \
    loop1.mp2 loop2.mp2 loop1.mp2 loop2.mp2 loop1.mp2 loop2.mp2 \
    > test2.mp2


mplayer test1.mp2
