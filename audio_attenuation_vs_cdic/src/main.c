#include <cdfm.h>
#include <csd.h>
#include <events.h>
#include <setsys.h>
#include <stdio.h>
#include <sysio.h>
#include <ucm.h>

#include "audio.h"
#include "graphics.h"
#include "hwreg.h"
#include "video.h"
#include <signal.h>

#define MPEG_SIG_PCB 0x1C00
#define PRINT_REGISTERS

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

#define REGDUMP_SIZE 1000
static unsigned long regdump[REGDUMP_SIZE][29];

static int regdump_index = 0;
static int recording_stopped = 0;
/* clang-format off */
static char regsize[]={
	16,16,16,16,32,
	32,16,16,
	/* Timestamp */
	32
};

/* clang-format on */

void print_registers() {
    int i, j;

    if (recording_stopped)
        return;
    recording_stopped = 1;

#ifdef PRINT_REGISTERS
    for (i = 0; i < regdump_index; i++) {
        printf("%3d ", i);
        for (j = 0; j <= 8; j++) {
            switch (regsize[j]) {
            case 0:
                printf(" %x", regdump[i][j]);
                break;
            case 8:
                printf(" %02x", regdump[i][j]);
                break;
            case 16:
                printf(" %04x", regdump[i][j]);
                break;
            case 32:
                printf(" %08x", regdump[i][j]);
                break;
            }
        }

        printf("\n");
    }
#endif
}

unsigned char *cdapdriv_static = 0;

void FindCdapDriverStruct() {
    int i;
    unsigned short driv_dbuf;
    unsigned long long_xbuf_irqfunc;

    if (cdapdriv_static != 0)
        return;

    for (i = 0x00df0000; i < 0x00dff000; i += 4) {
        if ((*(unsigned long *)i) == 0x00300000) {
            cdapdriv_static = (unsigned long *)i;
            long_xbuf_irqfunc = *(unsigned long *)(cdapdriv_static + 0x08e);

            driv_dbuf = *(unsigned short *)(cdapdriv_static + 0x92);
            /*
            printf("Found cdapdriv at %lx %x %x %lx %lx\n", i,
                   *(unsigned short *)(cdapdriv_static + 0x92),
                   *(unsigned short *)(cdapdriv_static + 0x12c),
                   *(unsigned long *)(cdapdriv_static + 0x08e),
                   *(unsigned long *)(cdapdriv_static + 0x128));
            */

            if (long_xbuf_irqfunc == 0x42a36c) {
                return;
            }
        }
    }
}

static unsigned long last_dclk = 0;

void recordstate() {

    static unsigned short last_driv_dbuf;
    static unsigned short last_driv_audctl;
    static unsigned short last_driv_nextabuf;
    static unsigned short last_cdic_achan;
    unsigned short last_coding0;
    unsigned short last_coding1;

    if (cdapdriv_static) {
        unsigned short driv_dbuf = *(unsigned short *)(cdapdriv_static + 0x92);
        unsigned short driv_audctl =
            *(unsigned short *)(cdapdriv_static + 0x12c);
        unsigned short driv_nextabuf =
            *(unsigned short *)(cdapdriv_static + 0x13a);
        unsigned short cdic_achan = CDIC_ACHAN;

        unsigned long long_xbuf_irqfunc =
            *(unsigned long *)(cdapdriv_static + 0x08e);
        unsigned long long_abuf_irqfunc =
            *(unsigned long *)(cdapdriv_static + 0x128);

        unsigned short coding0 = *(unsigned short *)0x30280a;
        unsigned short coding1 = *(unsigned short *)0x30320a;

        /* ABUF IRQ Handler is typically 0042b73c on cdi220b.rom */
        /* DBUF IRQ Handler is typically 0042a36c on cdi220b.rom */

        unsigned long dclk = FMA_DCLK;

        unsigned long dclkdiff = dclk - last_dclk;

        if ((last_driv_dbuf != driv_dbuf) ||
            (last_driv_audctl != driv_audctl) ||
            (last_driv_nextabuf != driv_nextabuf)) {

            regdump[regdump_index][0] = driv_dbuf;
            regdump[regdump_index][1] = driv_audctl;
            regdump[regdump_index][2] = driv_nextabuf;
            regdump[regdump_index][3] = cdic_achan;
            regdump[regdump_index][4] = long_xbuf_irqfunc;

            regdump[regdump_index][5] = long_abuf_irqfunc;
            regdump[regdump_index][6] = coding0;
            regdump[regdump_index][7] = coding1;

            regdump[regdump_index][8] = dclkdiff;

            regdump_index++;

            last_driv_dbuf = driv_dbuf;
            last_driv_audctl = driv_audctl;
            last_driv_nextabuf = driv_nextabuf;
            last_cdic_achan = cdic_achan;

            last_coding0 = coding0;
            last_coding1 = coding1;

            last_dclk = dclk;
        }
    }
}

void runProgram() {
    unsigned long atten;
    unsigned long i;
    unsigned long time0, time1, diff;

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

    FindCdapDriverStruct();

    /* Wait a second and record */
    time0 = FMA_DCLK;
    diff = FMA_DCLK - time0;
    while (diff < 45000) {
        diff = FMA_DCLK - time0;
        recordstate();
    }

    while ((CDIC_DBUF & 0xf) != 0x5)
        recordstate();

    for (i = 0; i < 4; i++) {
        startAudio(0x00800080);

        /* Wait a second and record */
        time0 = FMA_DCLK;
        diff = FMA_DCLK - time0;
        while (diff < 45000) {
            diff = FMA_DCLK - time0;
            recordstate();
        }
    }

    printf("Finished!\n");
    print_registers();

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
