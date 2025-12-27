# FMV Moving Windows with DCA signal manipulation

Loads a MPEG stream with a frame into memory and shows it using the DVC.
Only shows a cropped view of the MPEG footage and moves the cropped view around like a bouncing "DVD logo"
The time during the frame at which the position is set, can be configured using the gamepad.
Use Up/Down to move the signal on the DCA/LCT from line 0 to 279.

    dc_wrli(videoPath, lctB, sigpos * 2, 7, cp_sig());

The functions executed are these:

    DEBUG(mv_pos(mvPath, mvMapId, x * 2, y * 2, 1));
    DEBUG(mv_window(mvPath, mvMapId, x * 2, y * 2, 66 * 2, 44 * 2, 1));

There are certain observations to make on a 210/05+VMPEG:

* sigpos is defined as 0 <= sigpos <= 279 and can be controlled while the window is moving like a "DVD logo"
* The current position is printed to the terminal
* Both these functions are only atomic when called with `sigpos <= 240` or `sigpos >=266`. If `241 <= sigpos <= 265`, the
  movement of the window is stuttery.
* If `sigpos <= 240`, then the base case video dots are aligned to the edges in this example.
* If `sigpos == 0`, then the base case video dots are ahead of the MPEG video.
  * This makes sense because these are directly overwritten in software without waiting
* If `sigpos == 236` everything is perfectly aligned
* If `sigpos == 279`, then the base case video dots are ahead of the MPEG video.

The base image is this:

![Picture of a parrot](parrot.png)

## Preparing MPEG file

Execute `./create_movie_file.sh`, which converts the PNG into an MPEG file.
Feel free to replace the PNG file with another image.