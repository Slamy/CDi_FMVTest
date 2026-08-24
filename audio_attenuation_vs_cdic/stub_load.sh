set -e

# Compile
WINEPATH=D:/DOS/BIN wine D:/dos/bin/bmake.exe clean
WINEPATH=D:/DOS/BIN wine D:/dos/bin/bmake.exe link_app

exec cdi-serial --port /dev/ttyUSB0 download build/cdictest.app \
  --address 8000 --end --reset --terminal --terminal-baud 9600 \
  --terminal-log log_vmpeg

# venv/bin/python tovcd.py
