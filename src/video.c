#include <strings.h>
#include <csd.h>
#include <sysio.h>
#include <ucm.h>
#include <stdio.h>
#include <memory.h>
#include "video.h"

int videoPath;
int fctA, fctB, lctA, lctB;
u_int fctBuffer[FCT_SIZE];
u_int lineSkip;
u_int pixelStart;

u_int *lctAddress (plane, lctid)
	int plane;
	int lctid;
{
	int lctDummy, lnkInstr;
	lctDummy = dc_crlct(videoPath, plane, 2, 0);
	/* link this dummy LCT to line 1 of our parameter LCT */
	dc_llnk(videoPath, lctDummy, 1, lctid, 1);
	/* read the link instruction written by dc_llnk */
	lnkInstr = dc_rdli(videoPath, lctDummy, 1, 7);
	/* delete the dummy LCT */
	dc_dllct(videoPath, lctDummy);
	/* the address is in the lower 24 bits of the link instr */
	return (u_int*) (lnkInstr & 0x00ffffff);
}

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
	fctA = initFCT(PA, FCT_SIZE);
	lctA = initLCT(PA, LCT_SIZE);
	dc_flnk(videoPath, fctA, lctA, 0);

	fctBuffer[0] = cp_icm(ICM_CLUT7, ICM_CLUT7, NM_1, EV_ON, CS_A); /* Use CLUT7 for plane A and B, 1 Matte, External Video On */
	fctBuffer[1] = cp_tci(MIX_OFF, TR_CKEY_T, TR_CKEY_T); /* Transparancy Color Key for Plane A and B */
	fctBuffer[2] = cp_po(PR_AB); /* Plane A in front of B */
	fctBuffer[3] = cp_bkcol(BK_BLACK, BK_LOW);  /* Backdrop Low Intensity Black */
	fctBuffer[4] = cp_tcol(PA, 0, 0, 0); /* Set transparancy color to black: rgb(0,0,0) */
	fctBuffer[5] = cp_mcol(PA, 0, 0, 0); /* Set mask color to black: rgb(0,0,0) */
	fctBuffer[6] = cp_yuv(PA, 16, 128, 128); /* Set DYUV start value */
	fctBuffer[7] = cp_phld(PA, PH_OFF, 1); /* Set Mosaic (pixel_hold) off, size = 1 */
	fctBuffer[8] = cp_icf(PA, ICF_MIN); /* Min Image Contributing Factor */
	fctBuffer[9] = cp_matte(0, MO_END, MF_MF0, ICF_MAX, 0);
	fctBuffer[10] = cp_dprm(RMS_NORMAL, PRF_X2, BP_NORMAL);  /* Reload Display Parameters */
	
	dc_wrfct(videoPath, fctA, 0, 10, fctBuffer);
}

void setupPlaneB() {
	fctB = initFCT(PB, FCT_SIZE);
	lctB = initLCT(PB, LCT_SIZE);
	dc_flnk(videoPath, fctB, lctB, 0);

	fctBuffer[0] = cp_sig();
	fctBuffer[1] = cp_nop();
	fctBuffer[2] = cp_nop();
	fctBuffer[3] = cp_nop();
	fctBuffer[4] = cp_tcol(PB, 0, 0, 0); /* Set transparancy color to black: rgb(0,0,0) */
	fctBuffer[5] = cp_mcol(PB, 0, 0, 0); /* Set mask color to black: rgb(0,0,0) */
	fctBuffer[6] = cp_yuv(PB, 16, 128, 128); /* Set DYUV start value */
	fctBuffer[7] = cp_phld(PB, PH_OFF, 1); /* Set Mosaic (pixel_hold) off, size = 1 */
	fctBuffer[8] = cp_icf(PB, ICF_MIN); /* Min Image Contributing Factor */
	fctBuffer[9] = cp_nop();
	fctBuffer[10] = cp_dprm(RMS_NORMAL, PRF_X2, BP_NORMAL);  /* Reload Display Parameters */

	dc_wrfct(videoPath, fctB, 0, 10, fctBuffer);
}

void initVideo() {
	char *devName = csd_devname(DT_VIDEO, 1); /* Get Video Device Name */
	char *devParam;
    int videoMode;

	videoPath = open(devName, UPDAT_);        /* Open Video Device */
	devParam = csd_devparam(devName);

    videoMode = findstr(1, devParam, "LI=\"625\":") ? 0 : (findstr(1, devParam, "TV") ? 1 : 2) ; /* First parameter is first character to start searching at; 1-based, not 0-based! */
	/*printf("Video: %s %d\n", devParam, videoMode);*/
	free(devName); /* Release memory */
	free(devParam);

	/* Setup Video */
	if (videoMode == 0) { /* PAL - 384x280 */
		dc_setcmp(videoPath, 0);
		lineSkip = 0;
		pixelStart = 0;
	}
	else if (videoMode == 1) { /* NTSC TV - 384x240 */
		dc_setcmp(videoPath, 0);
		lineSkip = 20;
		pixelStart = lineSkip * SCREEN_WIDTH;
	}
	else { /* NTSC Monitor - 360x240 */
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
	close(videoPath);  /* Close Video Device */
}
