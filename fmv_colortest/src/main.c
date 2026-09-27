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

int mainSignal(sigCode)
int sigCode;
{
    if (sigCode == SIGINT) {
        printf("SIGINT!\n");
        exit_app = 1;
    } else {
        mpegSignal(sigCode);
    }
}

void initProgram() {
    /* Show MPEG only in the centre third, between the base-case bands. */
    dc_wrli(videoPath, lctA, 2*SCREEN_HEIGHT / 3, 0, cp_tci(MIX_OFF, TR_ON, TR_ON));
    dc_wrli(videoPath, lctA, 2*SCREEN_HEIGHT / 3 * 2, 0, cp_tci(MIX_OFF, TR_OFF, TR_OFF));
    setIcf(ICF_MAX, ICF_MAX);
}

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
        if (mpegStatus == MPP_STOP) {
            printf("Starting FMV\n");
            playMpeg();
        }
    }
}

extern int os9forkc();
extern char **environ;
char *argblk[] = {
    "vcd",
    0,
};

int main(argc, argv)
int argc;
char *argv[];
{
    /* system("vcd"); */
    int pid;
    /*
     * My VMPEG DVC starts in VCD mode.
     * When stub loading the application, it is not configured to Green Book resolution.
     * Manually starting the vcd application fixes this problem.
     */
    if ((pid = os9exec(os9forkc, argblk[0], argblk, environ, 0, 0, 3)) > 0)
        wait(0);
    else
        printf("cant fork\n");

    intercept(mainSignal);

    initSystem();
    runProgram();
    closeSystem();

    sleep(1);
    exit(0);
}
