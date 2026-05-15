#include <csd.h>
#include <events.h>
#include <setsys.h>
#include <stdio.h>
#include <sysio.h>
#include <ucm.h>

#include "graphics.h"
#include "hwreg.h"
#include "irq.h"
#include "mpeg.h"
#include "video.h"
#include <signal.h>

char do_fma_dma = 0;
unsigned int int_fma_dclk = 0;
unsigned short int_fma_status = 0;
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

void initSystem() {
    initVideo();
    initGraphics();
    initMpeg();
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
void take_system() {
    /* TODO I don't understand why this works for assembler code. thx to cdifan
     */
    store_a6();

    /* Switch to our IRQ handler */
    *((unsigned long *)0x1EC) = FMA_IRQ; /* vector delivered by CDIC */
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

/* MPEG-1 Pack has SCR starting at byte 4 */
unsigned long long pack_get_scr(unsigned char *buf) {
    unsigned long scr = 0;

    ASSERT(buf[0] == 0x00); /* ensure correct header */
    ASSERT(buf[1] == 0x00); /* ensure correct header */
    ASSERT(buf[2] == 0x01); /* ensure correct header */
    ASSERT(buf[3] == 0xBA); /* ensure correct header */

    ASSERT(buf[4] & 1); /* ensure marker bit */
    ASSERT(buf[6] & 1); /* ensure marker bit */
    ASSERT(buf[8] & 1); /* ensure marker bit */

    scr = ((unsigned long long)(buf[4] & 0x0E)) << 29;
    scr |= ((unsigned long long)buf[5]) << 22;
    scr |= ((unsigned long long)(buf[6] & 0xFE)) << 14;
    scr |= ((unsigned long long)buf[7]) << 7;
    scr |= ((unsigned long long)(buf[8] & 0xFE)) >> 1;

    return scr;
}

/* MPEG-1 Pack has SCR starting at byte 4 */
void pack_set_scr(unsigned char *buf, unsigned long long scr) {

    ASSERT(buf[0] == 0x00); /* ensure correct header */
    ASSERT(buf[1] == 0x00); /* ensure correct header */
    ASSERT(buf[2] == 0x01); /* ensure correct header */
    ASSERT(buf[3] == 0xBA); /* ensure correct header */

    ASSERT(buf[4] & 1); /* ensure marker bit */
    ASSERT(buf[6] & 1); /* ensure marker bit */
    ASSERT(buf[8] & 1); /* ensure marker bit */

    buf[4] = 0x21 | ((scr >> 29) & 0x0E); /* '01', SCR[32..30], marker */
    buf[5] = (scr >> 22) & 0xFF;
    buf[6] = 0x01 | ((scr >> 14) & 0xFE); /* SCR[21..15], marker */
    buf[7] = (scr >> 7) & 0xFF;
    buf[8] = 0x01 | ((scr << 1) & 0xFE); /* SCR[6..0], marker */
}

/* MPEG-1 Pack has SCR starting at byte 4 */
unsigned long long mpeg1_packet_get_pts() {}

void mpeg1_packet_get_dts() {}
static unsigned short last_int_fma_status = 0;

void runProgram() {
    unsigned long atten;
    unsigned long i;
    unsigned int times[2];
    unsigned int states[2];

    dc_ssig(videoPath, SIG_BLANK, 0);

    playMpeg(0x00800080); /* Normal L2L and R2R */

    fma_irq_occured = 0;
    take_system();

    /* Faking MA_Play */
    FMA_STRM = 0;
    FMA_R04 = 7; /* without this, playback is not possible*/
    FMA_IER = 0x013d; /* ignore CSU, bit 7 and bit 6 */
    FMA_CMD = 0x0002; /* start decoder */

    while (!fma_irq_occured && !exit_app)
        ;

    times[0] = int_fma_dclk;
    states[0] = int_fma_status;
    fma_irq_occured = 0;

    while (!fma_irq_occured && !exit_app)
        ;
    times[1] = int_fma_dclk;
    states[1] = int_fma_status;

    printf("%x %x %x\n", states[0], states[1], times[1] - times[0]);

    do_fma_dma = 1;

    /* print_registers(); */

    while (!exit_app) {

        if (fma_irq_occured) {
            fma_irq_occured = 0;
            if (int_fma_status != last_int_fma_status) {
                last_int_fma_status = int_fma_status;
                printf("%x\n", int_fma_status);
            }
        }
    }
}

/*
100 100 1c9
100 POLL
182 POLL + bit7 + CSU
100 POLL
44 bit6 + UPD
140 POLL + bit6
48 bit6 + Underflow
140 POLL + bit6
*/

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
