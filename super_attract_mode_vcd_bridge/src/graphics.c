#include <sysio.h>
#include <ucm.h>
#include <stdio.h>
#include <memory.h>
#include "video.h"
#include "graphics.h"
#include "config.h"

#ifdef CONFIG_MISTERLOGO
#include "misterlogo.h"
#endif

u_char *paVideo1;
u_char *paVideo2;

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

#ifdef CONFIG_MISTERLOGO
void copyRect(sourceBuffer, targetBuffer, x, y, width, height, sourceWidth)
	u_char *sourceBuffer,
	*targetBuffer;
u_short x, y, width, height, sourceWidth;
{
	register u_char *dst = targetBuffer + y * SCREEN_WIDTH + x;
	register u_char *src = sourceBuffer;
	register u_short h, w;
	register u_char tmp;

	for (h = 0; h < height; h++)
	{
		for (w = 0; w < width; w++)
		{
			*dst = *src;
			dst++;
			src++;
		}
		dst += SCREEN_WIDTH - width;
		src += sourceWidth - width;
	}
}
#endif

void createVideoBuffers()
{
	int x;

	paVideo1 = (u_char *)srqcmem(VBUFFER_SIZE, VIDEO1);
	paVideo2 = (u_char *)srqcmem(VBUFFER_SIZE, VIDEO2);

	if (!paVideo1)
	{
		printf("Memory fail 1!\n");
		exit(0);
	}
	if (!paVideo2)
	{
		printf("Memory fail 2!\n");
		exit(0);
	}

	/* fill with green to make the background transparent */
#ifdef CONFIG_MISTERLOGO
	fillVideoBuffer(paVideo1, 0x2d2d2d2d);
	fillVideoBuffer(paVideo2, 0);

	copyRect(mister_logo_pixels, paVideo1, 0, screen_height - MISTER_LOGO_HEIGHT, MISTER_LOGO_WIDTH, MISTER_LOGO_HEIGHT, MISTER_LOGO_WIDTH);
#endif

}

void clearRect(videoBuffer, x, y, width, height, color)
	u_char *videoBuffer;
u_short x, y, width, height;
u_char color;
{
	register u_int value = (color << 24) | (color << 16) | (color << 8) | color;
	register u_int *dst = (u_int *)(videoBuffer + y * SCREEN_WIDTH + x);
	register u_short h, w;

	width >>= 2;

	for (h = 0; h < height; h++)
	{
		for (w = 0; w < width; w++)
			*dst++ = value;
		dst += (SCREEN_WIDTH >> 2) - width;
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
}
