#include <cdfm.h>
#include <csd.h>
#include <events.h>
#include <setsys.h>
#include <stdio.h>
#include <sysio.h>
#include <ucm.h>

#include "audio.h"
#include "graphics.h"
#include "video.h"
#include <signal.h>

#define MPEG_SIG_PCB 0x1C00

extern int errno;

int exit_app = 0;

#define DEBUG(c)                                                               \
    if ((c) == -1) {                                                           \
        printf("FAIL: c (%d)\n", errno);                                       \
    }

static int musicRtf = -1;
static PCB musicPcb;

int mainSignal(sigCode)
int sigCode;
{
    if (sigCode == SIGINT) {
        printf("SIGINT!\n");
        exit_app = 1;
    } else if (sigCode == MPEG_SIG_PCB) {
        /* Occurs when playback has finished */
        printf("PCB %x %x %x\n", musicPcb.PCB_Stat, musicPcb.PCB_Sig);
    } else {
        printf("SIG %x!\n", sigCode);
    }
}

void initProgram() {}

void initSystem() {
    initVideo();
    initAudio();
    initGraphics();
    initProgram();
}

void closeSystem() { closeVideo(); }

void testVolume(unsigned long attenuation) {
    if (!exit_app) {
        sleep(1);
    }
}

void runProgram() {
    unsigned long atten;
    unsigned long i;

    /* Start playback from CD */

    musicRtf = open("/cd/zmusic.rtr", READ_);
    musicPcb.PCB_Video = NULL;
    musicPcb.PCB_Chan = 2;
    musicPcb.PCB_AChan = 2;
    musicPcb.PCB_Audio = NULL;
    musicPcb.PCB_Stat = 0;
    musicPcb.PCB_Data = NULL;
    musicPcb.PCB_Rec = 1; /* assume that there is only 1 EOR */
    musicPcb.PCB_Sig = MPEG_SIG_PCB;

    DEBUG(musicRtf >= 0);

    DEBUG(lseek(musicRtf, 0, 0));
    DEBUG(ss_play(musicRtf, &musicPcb));
    printf("Started Play %d\n", musicRtf);

    sleep(1);

    printf("Do sm_out()!\n");

    startAudio(0x00800080);
    sleep(1);
    startAudio(0x00800080);
    sleep(1);
    startAudio(0x00800080);
    sleep(1);
    startAudio(0x00800080);

    printf("Finished!\n");
    while (!exit_app) {
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
    intercept(mainSignal);

    initSystem();
    runProgram();
    closeSystem();

    sleep(1);
    exit(0);
}
