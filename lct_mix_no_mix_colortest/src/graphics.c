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

void createVideoBuffers() {
    setIcf(ICF_MIN, ICF_MIN);
    paCursor = (u_char *)srqcmem(VBUFFER_SIZE, VIDEO1);
    pbBackground = (u_char *)srqcmem(VBUFFER_SIZE, VIDEO2);

    buildImage(NULL, paCursor);
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
     * A has one raw CLUT bar per region. B repeats the 0/16/32 sequence in
     * each corresponding region; the non-NULL source selects that layout.
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
