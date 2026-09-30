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
    int i;

    for (i = 0; i < 64; i+=4) {
        dc_wrli(videoPath, lctA, i * 8 + 20, 0, cp_icf(PA, i));
    }
    dc_wrli(videoPath, lctA, i * 8 + 20, 0, cp_icf(PA, 0));

    dc_wrli(videoPath, lctA, 0, 7, cp_icf(PA, curIcfA));

    setIcf(ICF_MAX);
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
