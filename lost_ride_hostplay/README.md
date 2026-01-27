# Lost Ride Ambience Hostplay

Replicates system calls of "The Lost Ride" to play the rail road sound effect.

This example can be used to check emulation accuracy.

## Original recording

Recorded using the Verilator model of the MiSTer CD-i core

    Syscall @ ded9b2 8e I$SetStt 00000006 00000122 00000002 00000900 0000ab00 00001999 0000bca0 00000000  00df9340 00000000 00d0d1d8 00dffd90 00d0bc98 00d0bb26 00d08000 00dfd428 SetStt MA_Loop
    Syscall @ deda82 8e I$SetStt 00000006 00000124 00000002 00000000 0000bd00 00000000 0000ffff 00000000  00df9340 00040e20 00d0d1d8 00dffd90 00d03b56 00d0bb26 00d08000 00dfd428 SetStt MA_Play
    FMA Write Stream Number 1804 0000
    FMA IER 180e 013d
    FMA CMD 1800 0002
    Syscall @ ded8c6 8e I$SetStt 00000006 00000120 00000002 00800080 f5020000 00000000 0000bca0 00000000  00df9340 00000000 00d0d1d8 00dffd90 00d0bc98 00d0bb26 00d08000 00dfd428 SetStt MA_Cntrl

