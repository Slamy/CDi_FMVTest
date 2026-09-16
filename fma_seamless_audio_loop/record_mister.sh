# An old instance might be still running. A rare case but let's be sure
sshpass -p 1 ssh root@mister killall microcom

sshpass -p 1 ssh root@mister microcom /dev/ttyS1 -s 115200 | tee log_mister

