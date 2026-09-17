#include <csd.h>
#include <events.h>
#include <setsys.h>
#include <signal.h>
#include <stdio.h>
#include <sysio.h>
#include <ucm.h>


#include "graphics.h"
#include "mpeg.h"
#include "video.h"


#include <signal.h>

int exit_app = 0;

#define SIG_BLANK 0x0100

int mainSignal(sigCode)
int sigCode;
{
    if (sigCode == SIGINT) {
        printf("SIGINT!\n");
        exit_app = 1;
    } else if (sigCode == SIG_BLANK) {
        /* Song position advances only on MA_TRIG_UPD, never on VBLANK. */
        dc_ssig(videoPath, SIG_BLANK, 0);
    } else {
        mpegSignal(sigCode);
    }
}

void initProgram() {}

void initSystem() {
    initVideo();
    initGraphics();
    initMpeg();
    initProgram();
}

void closeSystem() { closeVideo(); }

void runProgram() {
    dc_ssig(videoPath, SIG_BLANK, 0);

    while (!exit_app) {
        /* The MPEG driver does not reliably signal every completed PCL on all
         * players, so also recycle the in-memory Program Stream once per VBL. */
        serviceMpeg();
        if (mpegStatus == MPP_STOP) {
            printf("Starting FMV\n");
            playMpeg();
        }
        tsleep(10);
    }
}

int main(argc, argv)
int argc;
char *argv[];
{
    intercept(mainSignal);
    initSystem();
    runProgram();
    closeSystem();

    sleep(1);
    exit(0);
}
