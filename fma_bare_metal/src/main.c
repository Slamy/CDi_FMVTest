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
#include "stereo_sine.h"
#include "video.h"
#include <signal.h>

char do_fma_dma = 0;
unsigned int int_fma_dclk = 0;
unsigned short int_fma_status = 0;
unsigned short dma_wordcnt = 0;
unsigned char *dma_addr = 0;

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

int vblank_cnt = 0;

int mainSignal(sigCode)
int sigCode;
{
    if (sigCode == SIGINT) {
        printf("SIGINT!\n");
        exit_app = 1;
    } else if (sigCode = SIG_BLANK) {
        dc_ssig(videoPath, SIG_BLANK, 0);
        vblank_cnt++;
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

#define REGDUMP_SIZE 600
static unsigned long regdump[REGDUMP_SIZE][20];
static int regdump_index = 0;

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

        /* clang-format off */
        if (regdump[i][1] & 0x001) printf(" EOI");
        if (regdump[i][1] & 0x002) printf(" CSU");
        if (regdump[i][1] & 0x004) printf(" UPD");
        if (regdump[i][1] & 0x008) printf(" UNF");
        if (regdump[i][1] & 0x010) printf(" DEC");
        if (regdump[i][1] & 0x020) printf(" ERR");
        if (regdump[i][1] & 0x040) printf(" bi6");
        if (regdump[i][1] & 0x080) printf(" bi7");
        if (regdump[i][1] & 0x100) printf(" POLL");
        /* clang-format on */

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
unsigned long mpeg1_packet_get_pts(unsigned char *buf) {
    unsigned long pts = 0;
    unsigned int i;
    buf += 11; /* skip MPEG-1 pack*/
    for (i = 0; i < 20; i++) {
        if (buf[0] == 0 && buf[1] == 0 && buf[2] == 1 && buf[3] == 0xC0)
            break;
        buf++;
    }

    if (!(buf[0] == 0 && buf[1] == 0 && buf[2] == 1 && buf[3] == 0xC0)) {
        return -1;
    }
    buf += 6;
    /* check for stuffing bytes*/
    if (*buf == 0xff)
        buf++;

    if (((*buf) & 0xC0) == 0x40) {
        buf += 2; /* skip std buffer size */
    }

    if (((*buf) & 0xE0) != 0x20) {
        return -2;
    }

    pts = ((unsigned long long)(buf[0] & 0x0E)) << 29;
    pts |= ((unsigned long long)buf[1]) << 22;
    pts |= ((unsigned long long)(buf[2] & 0xFE)) << 14;
    pts |= ((unsigned long long)buf[3]) << 7;
    pts |= ((unsigned long long)(buf[4] & 0xFE)) >> 1;

    return pts;
}

void mpeg1_packet_set_pts(unsigned char *buf, unsigned long long scr) {
    buf[0] = 0x21 | ((scr >> 29) & 0x0E); /* '01', SCR[32..30], marker */
    buf[1] = (scr >> 22) & 0xFF;
    buf[2] = 0x01 | ((scr >> 14) & 0xFE); /* SCR[21..15], marker */
    buf[3] = (scr >> 7) & 0xFF;
    buf[4] = 0x01 | ((scr << 1) & 0xFE); /* SCR[6..0], marker */
}

static unsigned short last_int_fma_status = 0;

void runProgram() {
    unsigned long atten;
    unsigned long i;
    unsigned int times[3];
    unsigned int states[3];
    unsigned long dma_transfer_dclk = 0;
    unsigned long upd_isr_dclk = 0;
    int magic_set = 0;

    dma_addr = stereo_sine_mpg;
    for (i = 0; i < 12; i++) {
        printf("pack %d\n", pack_get_scr(dma_addr));
        printf("pts %d\n", mpeg1_packet_get_pts(dma_addr));
        dma_addr += 2304;
    }

    /*                      1001101010110000    9ab0 */
    /* 10000100000000000000110011010101100001   0x21, 0x00, 0x03, 0x35, 0x61 */
    dc_ssig(videoPath, SIG_BLANK, 0);

    playMpeg(0x00800080); /* Normal L2L and R2R */

    take_system();

    /* Faking MA_Play */
    FMA_STRM = 0;
    FMA_R04 = 7;      /* without this, playback is not possible*/
    FMA_IER = 0x013d; /* ignore CSU, bit 7 and bit 6 */
    FMA_CMD = 0x0002; /* start decoder */

    fma_irq_occured = 0;
    while (!fma_irq_occured && !exit_app)
        ;
    times[0] = int_fma_dclk;
    states[0] = int_fma_status;

    fma_irq_occured = 0;
    while (!fma_irq_occured && !exit_app)
        ;
    times[1] = int_fma_dclk;
    states[1] = int_fma_status;

    fma_irq_occured = 0;
    while (!fma_irq_occured && !exit_app)
        ;
    times[2] = int_fma_dclk;
    states[2] = int_fma_status;

    printf("%x %x %d %d\n", states[0], states[1], times[1] - times[0],
           times[2] - times[1]);

    for (i = 0; i < 2; i++) {
        unsigned long now = FMA_DCLK;
        unsigned long playback_start_scr;
        unsigned long next_play_dclk;

        /*
        pack_set_scr(stereo_sine_mpg, now * 2);
        mpeg1_packet_set_pts(stereo_sine_mpg + 33, now * 2 + 40000);
        */

        dma_transfer_dclk = 0;
        upd_isr_dclk = 0;

        fma_irq_occured = 0;

        do_fma_dma = 1;
        dma_addr = stereo_sine_mpg;
        dma_wordcnt = 1152; /* always in packs of 2304 */
        playback_start_scr = FMA_DCLK - 30600;
        next_play_dclk = mpeg1_packet_get_pts(dma_addr) + playback_start_scr;

        vblank_cnt = 0;
        while (!exit_app) {
            if (fma_irq_occured) {
                fma_irq_occured = 0;

#if 1
                if (!do_fma_dma && int_fma_dclk >= next_play_dclk) {
                    if (!magic_set) {
                        magic_set = 1;
                        FMA_R04 = 0x1f;
                    }
                    dma_addr += 2304;
                    next_play_dclk =
                        mpeg1_packet_get_pts(dma_addr) + playback_start_scr;
                    do_fma_dma = 1;
                }
#endif

                if (regdump_index == 0) {
                    dma_transfer_dclk = int_fma_dclk;
                }
                if (int_fma_status & 0x4) {
                    upd_isr_dclk = int_fma_dclk;
                }

                if (regdump_index < REGDUMP_SIZE) {
                    regdump[regdump_index][0] = int_fma_dclk;
                    regdump[regdump_index][1] = int_fma_status;
                    regdump[regdump_index][2] = upd_isr_dclk;
                    regdump_index++;
                }
                /*
                ASSERT(!fma_irq_occured);*/
            }
        }

        FMA_CMD = 0x0001; /* stop decoder */

        printf("%ld\n", dma_transfer_dclk);
        printf("%ld\n", upd_isr_dclk);
        printf("%ld\n", (upd_isr_dclk - dma_transfer_dclk));

        FMA_STRM = 0;
        FMA_R04 = 7;      /* without this, playback is not possible*/
        FMA_IER = 0x013d; /* ignore CSU, bit 7 and bit 6 */
        FMA_CMD = 0x0002; /* start decoder */
    }

    /* print_registers(); */
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
