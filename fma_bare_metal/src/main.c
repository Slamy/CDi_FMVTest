#include <csd.h>
#include <events.h>
#include <setsys.h>
#include <stdio.h>
#include <sysio.h>
#include <ucm.h>

#include "graphics.h"
#include "hwreg.h"
#include "mpeg.h"
#include "video.h"
#include "irq.h"
#include <signal.h>

char do_fma_dma = 0;
unsigned int int_fma_dclk=0;
unsigned int int_fma_status=0;
char fma_irq_occured = 0;

#define ASSERT(c)                                                              \
    if (!(c)) {                                                                \
        printf("ASSERT FAIL\n");                                               \
    }

int exit_app = 0;
void poll_state();

unsigned short sigcodebuf[8];
unsigned short sigcodebuf_wrpos = 0;
unsigned short sigcodebuf_rdpos = 0;

int mainSignal(sigCode)
int sigCode;
{
    if (sigCode == SIGINT) {
        printf("SIGINT!\n");
        exit_app = 1;
    } else if ((sigCode & 0xf000) == MA_SIG_BASE) {
        /* printf("MA %x\n", sigCode); */
        sigcodebuf[sigcodebuf_wrpos] = sigCode;
        sigcodebuf_wrpos = (sigcodebuf_wrpos + 1) & 7;
        /* poll_state(); */

        if (sigCode & MA_TRIG_DEC)
            printf("D\n");
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

unsigned long *fmadrv_static = 0;

void FindFmaDriverStruct() {
    int i;

    if (fmadrv_static != 0)
        return;

    for (i = 0x00dfa000; i < 0x00dff000; i += 4) {
        if ((*(unsigned long *)i) == 0xe03000) {
            printf("Found fmadriv at %x\n", i);
            fmadrv_static = (unsigned long *)i;
        }
    }

    ASSERT(fmadrv_static);
}

static unsigned long regdump[200 * 3][20];
static int regdump_index = 0;

void poll_state() {
    unsigned long addr = *(unsigned long *)(((char *)fmadrv_static) + 0x122);
    /* unsigned short irqen = *(unsigned short *)(((char *)fmadrv_static) +
     * 0x120); */
    /* unsigned short irqs = *(unsigned short *)(((char *)fmadrv_static) +
     * 0x150) & ~0x0100; */
    unsigned long dclk = FMA_DCLK;

    static unsigned long last_addr;
    static unsigned short last_sigcode;
    static unsigned long last_dclk;

    if ((addr != last_addr) || (sigcodebuf_wrpos != sigcodebuf_rdpos)) {
        unsigned long dclkdiff = dclk - last_dclk;
        /* printf("Addr %lx %x %lx\n", dclkdiff, irqs, addr); */

        regdump[regdump_index][0] = dclkdiff;
        regdump[regdump_index][1] = (sigcodebuf_rdpos == sigcodebuf_wrpos)
                                        ? 0
                                        : sigcodebuf[sigcodebuf_rdpos];
        regdump[regdump_index][2] = addr;
        regdump_index++;

        last_addr = addr;
        /* last_sigcode = ma_sigcode; */
        last_dclk = dclk;

        if (sigcodebuf_wrpos != sigcodebuf_rdpos)
            sigcodebuf_rdpos = (sigcodebuf_rdpos + 1) & 7;
    }
}

/* Overwrite CDIC driver IRQ handling */
void take_system()
{
	/* TODO I don't understand why this works for assembler code. thx to cdifan */
	store_a6();

	/* Switch to our IRQ handler */
	*((unsigned long *)0x1EC) = FMA_IRQ;  /* vector delivered by CDIC */
}



void print_registers() {
    int i, j;

    for (i = 0; i < regdump_index; i++) {
        printf("%3d ", i);
        for (j = 0; j <= 2; j++) {
            printf(" %08x", regdump[i][j]);
        }

        printf("\n");
    }
}

void runProgram() {
    unsigned long atten;
    unsigned long i;

    dc_ssig(videoPath, SIG_BLANK, 0);

    playMpeg(0x00800080); /* Normal L2L and R2R */
    while (playback_has_ended == 0)
        poll_state();

        
    print_registers();

    while (!exit_app)
        ;
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
