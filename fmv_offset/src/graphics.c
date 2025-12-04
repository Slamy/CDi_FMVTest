#include <sysio.h>
#include <ucm.h>
#include <stdio.h>
#include <memory.h>
#include "video.h"
#include "graphics.h"

u_int frameDone = 0, frameTick = 0;

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
#define PIXEL_CORD(x, y) ((x) + (y) * SCREEN_WIDTH)

void setPixel(unsigned char *fb, int x, int y, int color)
{
	fb[PIXEL_CORD(x, y)] = color;
}

void draw2x2(unsigned char *fb, int x, int y, int color)
{
	setPixel(fb, x, y, color);
	setPixel(fb, x + 1, y, color);
	setPixel(fb, x, y + 1, color);
	setPixel(fb, x + 1, y + 1, color);
}

void drawRectangle(unsigned char *fb, int x, int y, int w, int h, int color)
{
	int i, j;

	/* Horizontal lines */
	for (i = x; i < x + w; i++)
	{
		setPixel(fb, i, y, color);
		setPixel(fb, i, y + h - 1, color);
	}

	/* Vertical lines */
	for (i = y; i < y + h; i++)
	{
		setPixel(fb, x, i, color);
		setPixel(fb, x + w - 1, i, color);
	}
}

void createVideoBuffers()
{
	int x;

	paVideo1 = (u_char *)srqcmem(VBUFFER_SIZE, VIDEO1);
	paVideo2 = (u_char *)srqcmem(VBUFFER_SIZE, VIDEO2);

	fillVideoBuffer(paVideo1, 0);
	fillVideoBuffer(paVideo2, 0);

#if 0
	/* a border with 1 pixel distance around the parrots eye */
	drawRectangle(paVideo1, (30 + 100) / 2 - 2, (30 + 100) / 2 - 2, 66 + 4, 44 + 4, 2);

	/* small rectangle in the center */
	drawRectangle(paVideo1, SCREEN_WIDTH / 2 - 1, SCREEN_HEIGHT / 2 - 1, 3, 3, 2);
#endif

	dc_wrli(videoPath, lctA, 0, 0, cp_dadr((int)paVideo1 + pixelStart));
	dc_wrli(videoPath, lctB, 0, 0, cp_dadr((int)paVideo2 + pixelStart));

	dc_wrli(videoPath, lctA, 0, 7, cp_icf(PA, ICF_MAX));
	dc_wrli(videoPath, lctB, 0, 7, cp_icf(PB, ICF_MAX));

	/* Valid starting with second line */
	dc_wrli(videoPath, lctA, 2, 6, cp_icm(ICM_CLUT7, ICM_CLUT7, NM_1, EV_ON, CS_A));
	dc_wrli(videoPath, lctA, 2, 7, cp_icf(PA, ICF_MAX));
	dc_wrli(videoPath, lctB, 2, 7, cp_icf(PB, ICF_MAX));
}

int readImage(file, videoBuffer)
int file;
u_char *videoBuffer;
{
	return read(file, videoBuffer, VBUFFER_SIZE);
}

int readScreen(file)
int file;
{
	return readImage(file, paVideo2);
}

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
			tmp = *src++;
			if (tmp)
			{
				*dst = tmp;
			}
			dst++;
		}
		dst += SCREEN_WIDTH - width;
		src += sourceWidth - width;
	}
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

void initGraphics()
{
	createVideoBuffers();
}
