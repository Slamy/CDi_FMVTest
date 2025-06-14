set -e

# Compile
WINEPATH=D:/DOS/BIN wine D:/dos/bin/bmake.exe link_app

# Stop previous execution if already running by sending Ctrl+C
stty -F /dev/ttyUSB0 9600 raw
echo -ne '\x03' > /dev/ttyUSB0
# The CD-i will now reset

# And go. Hopefully in time!
wine cdilink.exe -port 5 -n -a 8000 -d build/fmvtest.app -e

# Have a terminal
minicom -D /dev/ttyUSB0 -b 9600
