/* clang-format off */

#include <sysio.h>
#include <ucm.h>
#include <stdio.h>
#include <memory.h>
#include <errno.h>
#include "video.h"
#include "graphics.h"

/* clang-format on */

u_char *paCursor;
u_char *pbBackground;

int curIcfA = ICF_MAX;
int curIcfB = ICF_MAX;

/* DYUV's four-bit delta codes, interpreted as signed changes. */
static int dyuvDeltas[16] = {0, 1, 4, 9, 16, 27, 44, 79,
                             -128, -79, -44, -27, -16, -9, -4, -1};
static u_char planeABarLevels[TEST_LEVEL_COUNT] = {0, 16, 32, 64, 128,
                                                    192, 223, 235, 239, 255};

/* Choose the DYUV code that brings the luma closest to the requested level. */
int dyuvCodeForLevel(level, currentY) register int level;
register int currentY;
{
    register int code;
    register int bestCode = 0;
    register int bestError = 256;

    for (code = 0; code < 16; code++) {
        register int error = level - (currentY + dyuvDeltas[code]);
        if (error < 0)
            error = -error;
        if (error < bestError) {
            bestError = error;
            bestCode = code;
        }
    }
    return bestCode;
}

void buildPlaneA(target) register u_char *target;
{
    static u_char barWidths[TEST_LEVEL_COUNT] = {39, 38, 39, 38, 38,
                                                  39, 38, 39, 38, 38};
    register u_char *row = target;
    register u_int *from;
    register u_int *to;
    register int bar, code, currentY, x, y;

    /* DYUV starts each scanline at Y=16, U=128, V=128.  A zero high
     * nibble preserves neutral chroma; the low nibble controls luma. */
    currentY = 16;
    for (bar = 0; bar < TEST_LEVEL_COUNT; bar++) {
        for (x = 0; x < barWidths[bar]; x++) {
            code = dyuvCodeForLevel(planeABarLevels[bar], currentY);
            *row++ = code;
            currentY += dyuvDeltas[code];
        }
    }

    from = (u_int *)target;
    for (y = 1; y < SCREEN_HEIGHT; y++) {
        to = (u_int *)((u_char *)from + SCREEN_WIDTH);
        for (x = 0; x < SCREEN_WIDTH / sizeof(u_int); x++)
            *to++ = *from++;
        from = (u_int *)((u_char *)to - SCREEN_WIDTH);
    }
}

void createVideoBuffers() {
    setIcf(ICF_MIN, ICF_MIN);
    paCursor = (u_char *)srqcmem(VBUFFER_SIZE, VIDEO1);
    pbBackground = (u_char *)srqcmem(VBUFFER_SIZE, VIDEO2);

    buildPlaneA(paCursor);
    buildImage(pbBackground, pbBackground);

    dc_wrli(videoPath, lctA, 0, 0, cp_dadr((int)paCursor + pixelStart));
    dc_wrli(videoPath, lctB, 0, 0, cp_dadr((int)pbBackground + pixelStart));
}

void buildImage(source, target) register u_char *source;
register u_char *target;
{
    /*
     * These are the widths produced by (x * 10) / 384.  Do not calculate
     * that expression for every pixel: division is particularly expensive
     * during boot on the 68070.
     */
    static u_char barWidths[TEST_LEVEL_COUNT] = {39, 38, 39, 38, 38,
                                                  39, 38, 39, 38, 38};
    register u_char *row;
    register u_int *from;
    register u_int *to;
    register int bar, x, y;
    /*
     * B repeats the 0/16/32 CLUT sequence in each corresponding region.
     */
    row = target;
    for (bar = 0; bar < TEST_LEVEL_COUNT; bar++) {
        for (x = 0; x < barWidths[bar]; x++)
            *row++ = source ? ((x * 3) / barWidths[bar]) * 16 : bar;
    }

    /* SCREEN_WIDTH is word aligned: duplicate the scanline using longwords. */
    from = (u_int *)(row - SCREEN_WIDTH);
    for (y = 1; y < SCREEN_HEIGHT; y++) {
        to = (u_int *)((u_char *)from + SCREEN_WIDTH);
        for (x = 0; x < SCREEN_WIDTH / sizeof(u_int); x++)
            *to++ = *from++;
        from = (u_int *)((u_char *)to - SCREEN_WIDTH / sizeof(u_int) * sizeof(u_int));
    }

}

void setIcf(icfA, icfB) register int icfA, icfB;
{
    curIcfA = icfA > ICF_MAX ? ICF_MAX : (icfA < ICF_MIN ? ICF_MIN : icfA);
    curIcfB = icfB > ICF_MAX ? ICF_MAX : (icfB < ICF_MIN ? ICF_MIN : icfB);

    dc_wrli(videoPath, lctA, 0, 7, cp_icf(PA, curIcfA));
    dc_wrli(videoPath, lctB, 0, 7, cp_icf(PB, curIcfB));
}

void initGraphics() { createVideoBuffers(); }
