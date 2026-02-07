#include <strings.h>
#include <csd.h>
#include <sysio.h>
#include <ucm.h>
#include <stdio.h>
#include <memory.h>
#include "video.h"
#include "graphics.h"

int videoPath;
int fctA, fctB, lctA, lctB;
u_int fctBuffer[FCT_SIZE];
u_int lineSkip;
u_int pixelStart;
int screen_height;
int screen_width;

u_int *lctAddress(plane, lctid)
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
	return (u_int *)(lnkInstr & 0x00ffffff);
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

unsigned char mister_logo_palette[64][3] = {
	{0, 0, 0},
	{18, 3, 4},
	{8, 8, 8},
	{27, 4, 0},
	{33, 5, 0},
	{27, 9, 19},
	{41, 8, 0},
	{16, 17, 16},
	{53, 11, 0},
	{43, 18, 31},
	{28, 28, 28},
	{36, 36, 36},
	{63, 29, 47},
	{45, 45, 45},
	{83, 41, 63},
	{54, 54, 54},
	{99, 50, 76},
	{65, 65, 65},
	{118, 60, 90},
	{83, 84, 84},
	{142, 74, 109},
	{94, 95, 94},
	{101, 101, 101},
	{106, 106, 106},
	{166, 87, 128},
	{114, 114, 114},
	{185, 98, 143},
	{121, 121, 121},
	{124, 124, 124},
	{127, 128, 127},
	{130, 130, 130},
	{133, 133, 133},
	{137, 137, 137},
	{139, 139, 139},
	{142, 143, 143},
	{145, 145, 145},
	{148, 149, 149},
	{152, 152, 152},
	{154, 154, 154},
	{158, 158, 158},
	{161, 161, 161},
	{164, 165, 164},
	{167, 167, 167},
	{170, 171, 170},
	{178, 178, 178},
	{11, 255, 1},
	{187, 187, 187},
	{190, 190, 190},
	{194, 194, 194},
	{197, 197, 197},
	{200, 201, 200},
	{204, 204, 204},
	{207, 207, 207},
	{210, 211, 210},
	{213, 214, 213},
	{217, 217, 217},
	{220, 220, 220},
	{224, 224, 224},
	{227, 227, 227},
	{230, 230, 230},
	{233, 234, 233},
	{237, 237, 237},
	{239, 239, 239},
	{255, 255, 255}};

void setupPlaneA()
{
	int i = 0;
	int j;

	fctA = initFCT(PA, FCT_SIZE);
	lctA = initLCT(PA, LCT_SIZE);
	dc_flnk(videoPath, fctA, lctA, 0);

	fctBuffer[i++] = cp_icm(ICM_CLUT7, ICM_CLUT7, NM_1, EV_ON, CS_A); /* Use CLUT7 for plane A and B, 1 Matte, External Video On */
	fctBuffer[i++] = cp_tci(MIX_OFF, TR_CKEY_T, TR_ON);				  /* Transparancy Color Key for Plane A and B */
	fctBuffer[i++] = cp_po(PR_AB);									  /* Plane A in front of B */
	fctBuffer[i++] = cp_bkcol(BK_BLACK, BK_LOW);					  /* Backdrop Low Intensity Black */
	fctBuffer[i++] = cp_tcol(PA, 11, 255, 1);						  /* Set transparancy color to black: rgb(0,0,0) */
	fctBuffer[i++] = cp_mcol(PA, 0, 0, 0);							  /* Set mask color to black: rgb(0,0,0) */
	fctBuffer[i++] = cp_yuv(PA, 16, 128, 128);						  /* Set DYUV start value */
	fctBuffer[i++] = cp_phld(PA, PH_OFF, 1);						  /* Set Mosaic (pixel_hold) off, size = 1 */
	fctBuffer[i++] = cp_icf(PA, ICF_MAX);							  /* Min Image Contributing Factor */
	fctBuffer[i++] = cp_matte(0, MO_END, MF_MF0, ICF_MAX, 0);
	fctBuffer[i++] = cp_dprm(RMS_NORMAL, PRF_X2, BP_NORMAL); /* Reload Display Parameters */

	fctBuffer[i++] = cp_cbnk(0);
	for (j = 0; j < 64; j++)
	{
		fctBuffer[i++] = cp_clut(j, mister_logo_palette[j][0], mister_logo_palette[j][1], mister_logo_palette[j][2]);
	}

	fctBuffer[i++] = cp_dadr((int)paVideo1 + pixelStart);
	dc_wrfct(videoPath, fctA, 0, i, fctBuffer);
}

void setupPlaneB()
{
	fctB = initFCT(PB, FCT_SIZE);
	lctB = initLCT(PB, LCT_SIZE);
	dc_flnk(videoPath, fctB, lctB, 0);

	fctBuffer[0] = cp_sig();
	fctBuffer[1] = cp_nop();
	fctBuffer[2] = cp_nop();
	fctBuffer[3] = cp_nop();
	fctBuffer[4] = cp_tcol(PB, 0, 0, 0);	 /* Set transparancy color to black: rgb(0,0,0) */
	fctBuffer[5] = cp_mcol(PB, 0, 0, 0);	 /* Set mask color to black: rgb(0,0,0) */
	fctBuffer[6] = cp_yuv(PB, 16, 128, 128); /* Set DYUV start value */
	fctBuffer[7] = cp_phld(PB, PH_OFF, 1);	 /* Set Mosaic (pixel_hold) off, size = 1 */
	fctBuffer[8] = cp_icf(PB, ICF_MIN);		 /* Min Image Contributing Factor */
	fctBuffer[9] = cp_nop();
	fctBuffer[10] = cp_dprm(RMS_NORMAL, PRF_X2, BP_NORMAL); /* Reload Display Parameters */

	fctBuffer[11] = cp_dadr((int)paVideo2 + pixelStart);

	dc_wrfct(videoPath, fctB, 0, 12, fctBuffer);
}

void initVideo()
{
	char *devName = csd_devname(DT_VIDEO, 1); /* Get Video Device Name */
	char *devParam;
	int videoMode;

	videoPath = open(devName, UPDAT_); /* Open Video Device */
	devParam = csd_devparam(devName);

	videoMode = findstr(1, devParam, "LI=\"625\":") ? 0 : (findstr(1, devParam, "TV") ? 1 : 2); /* First parameter is first character to start searching at; 1-based, not 0-based! */
	/*printf("Video: %s %d\n", devParam, videoMode);*/
	free(devName); /* Release memory */
	free(devParam);

	lineSkip = 0;
	pixelStart = 0;

	/* Setup Video */
	if (videoMode == 0)
	{ /* PAL - 384x280 */
		dc_setcmp(videoPath, 0);
		screen_height = 280;
		screen_width = 384;
	}
	else if (videoMode == 1)
	{ /* NTSC TV - 384x240 */
		dc_setcmp(videoPath, 0);
		screen_height = 240;
		screen_width = 384;
	}
	else
	{ /* NTSC Monitor - 360x240 */
		dc_setcmp(videoPath, 1);
		screen_height = 240;
		screen_width = 360;
	}

	dc_intl(videoPath, 0); /* No interlace */

	gc_hide(videoPath); /* Hide the Graphics Cursor */

	initGraphics();

	setupPlaneA();
	setupPlaneB();
	dc_exec(videoPath, fctA, fctB);
}

void closeVideo()
{
	dc_dllct(videoPath, lctA);
	dc_dllct(videoPath, lctB);
	dc_dlfct(videoPath, fctA);
	dc_dlfct(videoPath, fctB);
	close(videoPath); /* Close Video Device */
}
