/* clang-format off */
#include <strings.h>
#include <csd.h>
#include <sysio.h>
#include <ucm.h>
#include <stdio.h>
#include <memory.h>
#include "video.h"
/* clang-format on */

int videoPath;
int fctA, fctB, lctA, lctB;
u_int fctBuffer[FCT_SIZE];
u_int lineSkip;
u_int pixelStart;

int initFCT(plane, size)
int plane;
int size;
{
    int fct = dc_crfct(videoPath, plane, size, 0);
    return fct;
}

int initLCT(plane, size)
int plane;
int size;
{
    int lct = dc_crlct(videoPath, plane, size, 0);
    dc_nop(videoPath, lct, 0, 0, size, 8); /* Fill all lines and cols of LCT with NOP instructions */
    return lct;
}

void setupPlaneA() {
    int i = 0;
    int j = 0;
    unsigned int c;

    fctA = initFCT(PA, FCT_SIZE);
    lctA = initLCT(PA, LCT_SIZE);
    dc_flnk(videoPath, fctA, lctA, 0);

    fctBuffer[i++] =
        cp_icm(ICM_CLUT7, ICM_CLUT7, NM_1, EV_ON, CS_A); /* Use CLUT7 for plane A and B, 1 Matte, External Video Off */
    fctBuffer[i++] = cp_tci(MIX_OFF, TR_OFF, TR_OFF);    /* DVC is visible in the top third */
    fctBuffer[i++] = cp_po(PR_AB);                       /* Plane A in front of B */
    fctBuffer[i++] = cp_bkcol(BK_BLACK, BK_LOW);         /* Backdrop Low Intensity Black */
    fctBuffer[i++] = cp_tcol(PA, 0, 0, 0);               /* Set transparancy color to black: rgb(0,0,0) */
    fctBuffer[i++] = cp_mcol(PA, 0, 0, 0);               /* Set mask color to black: rgb(0,0,0) */
    fctBuffer[i++] = cp_yuv(PA, 16, 128, 128);           /* Set DYUV start value */
    fctBuffer[i++] = cp_phld(PA, PH_OFF, 1);             /* Set Mosaic (pixel_hold) off, size = 1 */
    fctBuffer[i++] = cp_icf(PA, ICF_MIN);                /* Min Image Contributing Factor */
    fctBuffer[i++] = cp_matte(0, MO_END, MF_MF0, ICF_MAX, 0);
    fctBuffer[i++] = cp_dprm(RMS_NORMAL, PRF_X2, BP_NORMAL); /* Reload Display Parameters */

    fctBuffer[i++] = cp_cbnk(0);

    for (j = 0; j < 64; j++) {
        c = cp_clut(j, j, j, j);

        if (j < TEST_LEVEL_COUNT) {
            c = cp_clut(j, calibratedBaseLevels[j], calibratedBaseLevels[j], calibratedBaseLevels[j]);
        }

        fctBuffer[i++] = c;
    }

    fctBuffer[i++] = cp_cbnk(1);

    for (j = 0; j < 64; j++) {
        c = cp_clut(j, j, j, j);
        if (j < TEST_LEVEL_COUNT)
            c = cp_clut(j, uncalibratedBaseLevels[j], uncalibratedBaseLevels[j], uncalibratedBaseLevels[j]);

        fctBuffer[i++] = c;
    }

    dc_wrfct(videoPath, fctA, 0, i, fctBuffer);
}

void setupPlaneB() {
    fctB = initFCT(PB, FCT_SIZE);
    lctB = initLCT(PB, LCT_SIZE);
    dc_flnk(videoPath, fctB, lctB, 0);

    fctBuffer[0] = cp_nop();
    fctBuffer[1] = cp_nop();
    fctBuffer[2] = cp_nop();
    fctBuffer[3] = cp_nop();
    fctBuffer[4] = cp_tcol(PB, 0, 0, 0);     /* Set transparancy color to black: rgb(0,0,0) */
    fctBuffer[5] = cp_mcol(PB, 0, 0, 0);     /* Set mask color to black: rgb(0,0,0) */
    fctBuffer[6] = cp_yuv(PB, 16, 128, 128); /* Set DYUV start value */
    fctBuffer[7] = cp_phld(PB, PH_OFF, 1);   /* Set Mosaic (pixel_hold) off, size = 1 */
    fctBuffer[8] = cp_icf(PB, ICF_MIN);      /* Min Image Contributing Factor */
    fctBuffer[9] = cp_nop();
    fctBuffer[10] = cp_dprm(RMS_NORMAL, PRF_X2, BP_NORMAL); /* Reload Display Parameters */

    dc_wrfct(videoPath, fctB, 0, 10, fctBuffer);
}

void initVideo() {
    char *devName = csd_devname(DT_VIDEO, 1); /* Get Video Device Name */
    char *devParam;
    int videoMode;

    videoPath = open(devName, UPDAT_); /* Open Video Device */
    devParam = csd_devparam(devName);

    videoMode = findstr(1, devParam, "LI=\"625\":")
                    ? 0
                    : (findstr(1, devParam, "TV")
                           ? 1
                           : 2); /* First parameter is first character to start searching at; 1-based, not 0-based! */
    /*printf("Video: %s %d\n", devParam, videoMode);*/
    free(devName); /* Release memory */
    free(devParam);

    /* Setup Video */
    if (videoMode == 0) { /* PAL - 384x280 */
        dc_setcmp(videoPath, 0);
        lineSkip = 0;
        pixelStart = 0;
    } else if (videoMode == 1) { /* NTSC TV - 384x240 */
        dc_setcmp(videoPath, 0);
        lineSkip = 20;
        pixelStart = lineSkip * SCREEN_WIDTH;
    } else { /* NTSC Monitor - 360x240 */
        dc_setcmp(videoPath, 1);
        lineSkip = 20;
        pixelStart = 20 * SCREEN_WIDTH;
    }

    dc_intl(videoPath, 0); /* No interlace */

    gc_hide(videoPath); /* Hide the Graphics Cursor */

    setupPlaneA();
    setupPlaneB();
    dc_exec(videoPath, fctA, fctB);
}

void closeVideo() {
    dc_dllct(videoPath, lctA);
    dc_dllct(videoPath, lctB);
    dc_dlfct(videoPath, fctA);
    dc_dlfct(videoPath, fctB);
    close(videoPath); /* Close Video Device */
}
