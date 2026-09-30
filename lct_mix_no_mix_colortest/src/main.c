#include <csd.h>
#include <events.h>
#include <mv.h>
#include <setsys.h>
#include <signal.h>
#include <stdio.h>
#include <sysio.h>
#include <ucm.h>

#include "graphics.h"
#include "video.h"

int exit_app = 0;
extern int errno;

int mainSignal(sigCode)
int sigCode;
{
    if (sigCode == SIGINT) {
        printf("SIGINT!\n");
        exit_app = 1;
    }
}

#define DEBUG(c)                                                                                                       \
    if ((c) == -1) {                                                                                                   \
        printf("FAIL: c (%d)\n", errno);                                                                               \
    }

void initProgram() {
    int maPath, mvPath, maMapId, mvMapId;

    char *devName = csd_devname(DT_MPEGV, 1); /* Get MPEG Video Device Name */
    mvPath = open(devName, 0);                /* Open MPEG Video Device */
    free(devName);

    DEBUG(mv_borcol(mvPath, mvMapId, 0, 0, 0));

    /* LCT changes: Plane A only, Plane B only, then both planes mixed. */
    dc_wrli(videoPath, lctA, SCREEN_HEIGHT * 2 / 3, 0, cp_tci(MIX_OFF, TR_ON, TR_ON));
    dc_wrli(videoPath, lctA, SCREEN_HEIGHT * 2 / 3 * 2, 1, cp_icm(ICM_OFF, ICM_OFF, NM_1, EV_OFF, CS_A));

    dc_wrli(videoPath, lctA, SCREEN_HEIGHT * 2 / 3 * 2, 0, cp_icm(ICM_OFF, ICM_OFF, NM_1, EV_OFF, CS_A));

    dc_wrli(videoPath, lctA, SCREEN_HEIGHT * 2 / 3 * 2 + 16 * 1, 0, cp_bkcol(BK_LOW, BK_WHITE));
    dc_wrli(videoPath, lctA, SCREEN_HEIGHT * 2 / 3 * 2 + 16 * 2, 0, cp_bkcol(BK_HIGH, BK_WHITE));
    dc_wrli(videoPath, lctA, SCREEN_HEIGHT * 2 / 3 * 2 + 16 * 3, 0, cp_bkcol(BK_LOW, BK_RED));
    dc_wrli(videoPath, lctA, SCREEN_HEIGHT * 2 / 3 * 2 + 16 * 4, 0, cp_bkcol(BK_HIGH, BK_RED));
    dc_wrli(videoPath, lctA, SCREEN_HEIGHT * 2 / 3 * 2 + 16 * 5, 0, cp_bkcol(BK_LOW, BK_BLACK));
    dc_wrli(videoPath, lctA, SCREEN_HEIGHT * 2 / 3 * 2 + 16 * 6, 0, cp_bkcol(BK_HIGH, BK_BLACK));

    setIcf(ICF_MAX, ICF_MAX);
}

void initSystem() {
    initVideo();
    initGraphics();
    initProgram();
}

void closeSystem() { closeVideo(); }

void runProgram() {
    while (!exit_app)
        ;
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
