#include <sysio.h>
#include <ucm.h>
#include <stdio.h>
#include <memory.h>
#include "video.h"
#include "graphics.h"

u_int frameDone = 0, frameTick = 0;

u_char *paCursor;
u_char *pbBackground;

int curIcfA = ICF_MAX;
int curIcfB = ICF_MAX;

void fillBuffer(buffer, data, size) register u_int *buffer;
register u_int data, size;
{
	int i;
	size = size >> 2;
	for (i = 0; i < size; i++)
	{
		*buffer++ = data;
	}
}

void fillVideoBuffer(videoBuffer, data) register u_int *videoBuffer;
u_int data;
{
	fillBuffer(videoBuffer, data, VBUFFER_SIZE);
}

void createVideoBuffers()
{
	setIcf(ICF_MIN, ICF_MIN);
	paCursor = (u_char *)srqcmem(VBUFFER_SIZE, VIDEO1);
	pbBackground = (u_char *)srqcmem(VBUFFER_SIZE, VIDEO2);

	dc_wrli(videoPath, lctA, 0, 0, cp_dadr((int)paCursor + pixelStart));
	dc_wrli(videoPath, lctB, 0, 0, cp_dadr((int)pbBackground + pixelStart));
}

void clearPalette()
{
	int line = FCT_PAL_START;
	int bank, color;

	for (bank = 0; bank < 2; bank++)
	{
		dc_wrfi(videoPath, fctA, line, cp_cbnk(bank + 0));
		dc_wrfi(videoPath, fctB, line, cp_cbnk(bank + 2));

		line++;

		for (color = 0; color < 64; color++)
		{
			dc_wrfi(videoPath, fctA, line, cp_clut(color, 0, 0, 0));
			dc_wrfi(videoPath, fctB, line, cp_clut(color, 0, 0, 0));
			line++;
		}
	}
}

void setIcf(icfA, icfB) register int icfA, icfB;
{
	curIcfA = icfA > ICF_MAX ? ICF_MAX : (icfA < ICF_MIN ? ICF_MIN : icfA);
	curIcfB = icfB > ICF_MAX ? ICF_MAX : (icfB < ICF_MIN ? ICF_MIN : icfB);

	dc_wrli(videoPath, lctA, 0, 7, cp_icf(PA, curIcfA));
	dc_wrli(videoPath, lctB, 0, 7, cp_icf(PB, curIcfB));
}

void initGraphics()
{
	createVideoBuffers();
	clearPalette();
}
