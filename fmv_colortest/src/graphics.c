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

/*
 * Direct-DVC levels calibrated against the MPEG output captured from this
 * CD-i after the grabber was tuned to video black/white. They correspond
 * respectively to the nominal MPEG source levels 0, 16, 32, 64, 128, 192,
 * 223, 235, 239, and 255.
 */
u_char calibratedBaseLevels[TEST_LEVEL_COUNT] = {18, 32, 46, 75, 132, 188, 215, 226, 229, 243};
u_char uncalibratedBaseLevels[TEST_LEVEL_COUNT] = {0, 16, 32, 64, 128, 192, 223, 235, 239, 255};

void fillBuffer(buffer, data, size) register u_int *buffer;
register u_int data, size;
{
    int i;
    size = size >> 2;
    for (i = 0; i < size; i++) {
        *buffer++ = data;
    }
}

void fillVideoBuffer(videoBuffer, data) register u_int *videoBuffer;
u_int data;
{
    fillBuffer(videoBuffer, data, VBUFFER_SIZE);
}

void createVideoBuffers() {
    setIcf(ICF_MIN, ICF_MIN);
    paCursor = (u_char *)srqcmem(VBUFFER_SIZE, VIDEO1);
    pbBackground = (u_char *)srqcmem(VBUFFER_SIZE, VIDEO2);

    fillVideoBuffer(pbBackground, 0);
    buildImage(NULL, paCursor);

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
    register int bar, x, y, height;

    /*
     * The uncalibrated base case (CLUT bank 1) occupies the top third. MPEG
     * occupies the middle third, and the calibrated base case (bank 0) is below.
     */
    row = target;
    height = SCREEN_HEIGHT / 3;
    for (bar = 0; bar < TEST_LEVEL_COUNT; bar++)
        for (x = barWidths[bar]; x; x--)
            *row++ = 64 + bar;

    /* SCREEN_WIDTH is word aligned: duplicate the scanline using longwords. */
    from = (u_int *)(row - SCREEN_WIDTH);
    for (y = 1; y < height; y++) {
        to = (u_int *)((u_char *)from + SCREEN_WIDTH);
        for (x = 0; x < SCREEN_WIDTH / sizeof(u_int); x++)
            *to++ = *from++;
        from = (u_int *)((u_char *)to - SCREEN_WIDTH / sizeof(u_int) * sizeof(u_int));
    }

    row = target + (SCREEN_HEIGHT / 3 * 2) * SCREEN_WIDTH;
    height = SCREEN_HEIGHT - SCREEN_HEIGHT / 3 * 2;
    for (bar = 0; bar < TEST_LEVEL_COUNT; bar++)
        for (x = barWidths[bar]; x; x--)
            *row++ = bar;

    from = (u_int *)(row - SCREEN_WIDTH);
    for (y = 1; y < height; y++) {
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
