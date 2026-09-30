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

int curIcfA = ICF_MAX;

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
    setIcf(ICF_MIN);
    paCursor = (u_char *)srqcmem(VBUFFER_SIZE, VIDEO1);

    buildPlaneA(paCursor);

    dc_wrli(videoPath, lctA, 0, 0, cp_dadr((int)paCursor + pixelStart));
}

void setIcf(icfA) register int icfA;
{
    curIcfA = icfA > ICF_MAX ? ICF_MAX : (icfA < ICF_MIN ? ICF_MIN : icfA);

    dc_wrli(videoPath, lctA, 0, 7, cp_icf(PA, curIcfA));
}

void initGraphics() { createVideoBuffers(); }
