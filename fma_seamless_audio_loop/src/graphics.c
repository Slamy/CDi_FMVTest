/* clang-format off */

#include <sysio.h>
#include <ucm.h>
#include <stdio.h>
#include <memory.h>
#include "video.h"
#include "graphics.h"

/* clang-format on */

u_int frameDone = 0, frameTick = 0;

u_char *paVideo1;
u_char *paVideo2;

int curIcfA = ICF_MAX;
int curIcfB = ICF_MAX;

/* The audible map is: intro, restart, four blue loops and three green loops.
 * Timestamps are in the MPEG 90 kHz clock domain. */
#define INTRO_TICKS 7053UL
#define RESTART_TICKS 84637UL
#define LOOP_TICKS 86988UL
#define REPEAT_TICKS (RESTART_TICKS + 7UL * LOOP_TICKS)
#define FIRST_PASS_TICKS (INTRO_TICKS + REPEAT_TICKS)

#define START_SECTORS 1
#define RESTART_SECTORS 10
#define INTRO_RESTART_SECTORS (START_SECTORS + RESTART_SECTORS)
#define LOOP1_SECTORS 44
#define LOOP2_SECTORS 33
#define LOOP_FILE_SECTORS 11
#define SECTOR_COUNT (INTRO_RESTART_SECTORS + LOOP1_SECTORS + LOOP2_SECTORS)
#define SECTOR_PITCH 3 /* One coloured sector column plus one black gutter. */

#define MAP_W (SECTOR_COUNT * SECTOR_PITCH - 1)
#define MAP_X ((SCREEN_WIDTH - MAP_W) / 2)
#define MAP_Y 208
#define MAP_H 14
#define MPEG_FRAME_TICKS 2351UL
#define BEAT_PHASE_TICKS (2UL * MPEG_FRAME_TICKS)
#define BEAT_PULSE_TICKS (3UL * BEAT_PHASE_TICKS)

static unsigned long songTicks;
static int firstPass;
static int lastPlayhead = -1;
static int glow1 = -1;
static int glow2 = -1;
static int glow3 = -1;
static int mapLabelPhase = -1;

typedef struct {
    char character;
    unsigned char row[7];
} BitmapGlyph;

/* Deliberately tiny built-in alphabet for the static label below. */
static BitmapGlyph labelGlyphs[] = {
    {'A', {0x0e, 0x11, 0x11, 0x1f, 0x11, 0x11, 0x11}},
    {'B', {0x1e, 0x11, 0x11, 0x1e, 0x11, 0x11, 0x1e}},
    {'C', {0x0e, 0x11, 0x10, 0x10, 0x10, 0x11, 0x0e}},
    {'D', {0x1e, 0x11, 0x11, 0x11, 0x11, 0x11, 0x1e}},
    {'E', {0x1f, 0x10, 0x10, 0x1e, 0x10, 0x10, 0x1f}},
    {'F', {0x1f, 0x10, 0x10, 0x1e, 0x10, 0x10, 0x10}},
    {'G', {0x0e, 0x11, 0x10, 0x17, 0x11, 0x11, 0x0e}},
    {'H', {0x11, 0x11, 0x11, 0x1f, 0x11, 0x11, 0x11}},
    {'I', {0x0e, 0x04, 0x04, 0x04, 0x04, 0x04, 0x0e}},
    {'K', {0x11, 0x12, 0x14, 0x18, 0x14, 0x12, 0x11}},
    {'L', {0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x1f}},
    {'M', {0x11, 0x1b, 0x15, 0x11, 0x11, 0x11, 0x11}},
    {'N', {0x11, 0x19, 0x15, 0x13, 0x11, 0x11, 0x11}},
    {'O', {0x0e, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0e}},
    {'P', {0x1e, 0x11, 0x11, 0x1e, 0x10, 0x10, 0x10}},
    {'R', {0x1e, 0x11, 0x11, 0x1e, 0x14, 0x12, 0x11}},
    {'S', {0x0f, 0x10, 0x10, 0x0e, 0x01, 0x01, 0x1e}},
    {'T', {0x1f, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04}}
    ,{'U', {0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0e}}
    ,{'W', {0x11, 0x11, 0x11, 0x15, 0x15, 0x15, 0x0a}}
    ,{'Y', {0x11, 0x11, 0x0a, 0x04, 0x04, 0x04, 0x04}}
    ,{'6', {0x0e, 0x10, 0x10, 0x1e, 0x11, 0x11, 0x0e}}
    ,{'7', {0x1f, 0x01, 0x02, 0x04, 0x08, 0x08, 0x08}}
    ,{'-', {0x00, 0x00, 0x00, 0x1f, 0x00, 0x00, 0x00}}
    ,{'.', {0x00, 0x00, 0x00, 0x00, 0x00, 0x0c, 0x0c}}
    ,{'a', {0x00, 0x0e, 0x01, 0x0f, 0x11, 0x11, 0x0f}}
    ,{'b', {0x10, 0x10, 0x1e, 0x11, 0x11, 0x11, 0x1e}}
    ,{'c', {0x00, 0x0e, 0x11, 0x10, 0x10, 0x11, 0x0e}}
    ,{'d', {0x01, 0x01, 0x0f, 0x11, 0x11, 0x11, 0x0f}}
    ,{'e', {0x00, 0x0e, 0x11, 0x1f, 0x10, 0x11, 0x0e}}
    ,{'f', {0x06, 0x08, 0x1e, 0x08, 0x08, 0x08, 0x08}}
    ,{'g', {0x00, 0x0f, 0x11, 0x11, 0x0f, 0x01, 0x0e}}
    ,{'h', {0x10, 0x10, 0x1e, 0x11, 0x11, 0x11, 0x11}}
    ,{'i', {0x04, 0x00, 0x0c, 0x04, 0x04, 0x04, 0x0e}}
    ,{'j', {0x02, 0x00, 0x06, 0x02, 0x02, 0x12, 0x0c}}
    ,{'k', {0x10, 0x10, 0x12, 0x14, 0x18, 0x14, 0x12}}
    ,{'l', {0x0c, 0x04, 0x04, 0x04, 0x04, 0x04, 0x0e}}
    ,{'m', {0x00, 0x1a, 0x15, 0x15, 0x11, 0x11, 0x11}}
    ,{'n', {0x00, 0x1e, 0x11, 0x11, 0x11, 0x11, 0x11}}
    ,{'o', {0x00, 0x0e, 0x11, 0x11, 0x11, 0x11, 0x0e}}
    ,{'p', {0x00, 0x1e, 0x11, 0x11, 0x1e, 0x10, 0x10}}
    ,{'q', {0x00, 0x0f, 0x11, 0x11, 0x0f, 0x01, 0x01}}
    ,{'r', {0x00, 0x16, 0x19, 0x10, 0x10, 0x10, 0x10}}
    ,{'s', {0x00, 0x0f, 0x10, 0x0e, 0x01, 0x01, 0x1e}}
    ,{'t', {0x08, 0x08, 0x1e, 0x08, 0x08, 0x09, 0x06}}
    ,{'u', {0x00, 0x11, 0x11, 0x11, 0x11, 0x13, 0x0d}}
    ,{'v', {0x00, 0x11, 0x11, 0x11, 0x0a, 0x0a, 0x04}}
    ,{'w', {0x00, 0x11, 0x11, 0x15, 0x15, 0x15, 0x0a}}
    ,{'x', {0x00, 0x11, 0x0a, 0x04, 0x04, 0x0a, 0x11}}
    ,{'y', {0x00, 0x11, 0x11, 0x0f, 0x01, 0x11, 0x0e}}
    ,{'z', {0x00, 0x1f, 0x02, 0x04, 0x08, 0x10, 0x1f}}
};

#define LABEL_GLYPH_COUNT (sizeof(labelGlyphs) / sizeof(labelGlyphs[0]))

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
/* Draw a scanline by walking the framebuffer.  The expensive y * width
 * address calculation is done by the caller once, not for every pixel. */
static void drawLine(dst, width, color) register u_char *dst;
register int width;
register int color;
{
    while (width--)
        *dst++ = color;
}

static void fillRect(fb, x, y, w, h, color) u_char *fb;
int x, y, w, h, color;
{
    register u_char *dst;

    dst = fb + y * SCREEN_WIDTH + x;
    while (h--) {
        drawLine(dst, w, color);
        dst += SCREEN_WIDTH;
    }
}

static BitmapGlyph *findLabelGlyph(character) char character;
{
    int i;
    for (i = 0; i < LABEL_GLYPH_COUNT; i++)
        if (labelGlyphs[i].character == character)
            return &labelGlyphs[i];
    return NULL;
}

static void drawLabelGlyph(fb, x, y, glyph, color) u_char *fb;
int x, y, color;
BitmapGlyph *glyph;
{
    register u_char *dst;
    register unsigned char bits;
    int row, column;

    for (row = 0; row < 7; row++) {
        dst = fb + (y + row) * SCREEN_WIDTH + x;
        bits = glyph->row[row];
        for (column = 0; column < 5; column++) {
            if (bits & (0x10 >> column))
                *dst = color;
            dst++;
        }
    }
}

static void drawLabel(fb, x, y, text, color) u_char *fb;
int x, y, color;
char *text;
{
    BitmapGlyph *glyph;
    char character;
    while (*text) {
        character = *text;
        if (character == ' ')
            x += 4;
        else if ((glyph = findLabelGlyph(character)) != NULL) {
            drawLabelGlyph(fb, x, y, glyph, color);
            x += 6;
        }
        text++;
    }
}

static int labelWidth(text) char *text;
{
    int width = 0;
    while (*text++)
        width += (text[-1] == ' ') ? 4 : 6;
    return width;
}

static void drawCenteredLabel(fb, y, text, color) u_char *fb;
int y, color;
char *text;
{
    drawLabel(fb, (SCREEN_WIDTH - labelWidth(text)) / 2, y, text, color);
}

static void drawPlayhead(fb, x) u_char *fb;
int x;
{
    /* One bright marker sits above the currently audible sector. */
    fillRect(fb, x, MAP_Y - 6, 1, 6, 7);
}

static void erasePlayhead(fb, x) u_char *fb;
int x;
{
    fillRect(fb, x, MAP_Y - 6, 1, 6, 0);
}

static void drawAfterglow(fb, x, color) u_char *fb;
int x, color;
{
    fillRect(fb, x, MAP_Y - 6, 1, 6, color);
}

static void drawSectorColumn(fb, x, color) u_char *fb;
int x, color;
{
    register u_char *dst = fb + (MAP_Y + 1) * SCREEN_WIDTH + x;
    register int height = MAP_H - 2;
    while (height--) {
        *dst = color;
        dst += SCREEN_WIDTH;
    }
}

static void drawFileBar(fb, firstSector, sectorCount, color) u_char *fb;
int firstSector, sectorCount, color;
{
    /* Fill sector gutters within a file; leave a one-pixel gap between files. */
    fillRect(fb, MAP_X + firstSector * SECTOR_PITCH, MAP_Y + MAP_H + 2,
             sectorCount * SECTOR_PITCH - 1, 2, color);
}

static void drawStaticMap(fb) u_char *fb;
{
    int sector, file;

    /* Each of the 88 generated MPEG sectors owns exactly one screen column. */
    for (sector = 0; sector < INTRO_RESTART_SECTORS; sector++)
        drawSectorColumn(fb, MAP_X + sector * SECTOR_PITCH, 3);
    for (; sector < INTRO_RESTART_SECTORS + LOOP1_SECTORS; sector++)
        drawSectorColumn(fb, MAP_X + sector * SECTOR_PITCH, 5);
    for (; sector < SECTOR_COUNT; sector++)
        drawSectorColumn(fb, MAP_X + sector * SECTOR_PITCH, 6);

    /* Horizontal bars reveal the boundaries of the independent MPEG files. */
    drawFileBar(fb, 0, START_SECTORS, 3);
    drawFileBar(fb, START_SECTORS, RESTART_SECTORS, 3);
    sector = INTRO_RESTART_SECTORS;
    for (file = 0; file < 4; file++) {
        drawFileBar(fb, sector, LOOP_FILE_SECTORS, 5);
        sector += LOOP_FILE_SECTORS;
    }
    for (file = 0; file < 3; file++) {
        drawFileBar(fb, sector, LOOP_FILE_SECTORS, 6);
        sector += LOOP_FILE_SECTORS;
    }
}

static void drawPanel(fb) u_char *fb;
{
    /* Draw just the frame edges; the buffer is already black. */
    fillRect(fb, 24, 164, 336, 1, 4);
    fillRect(fb, 24, 229, 336, 1, 4);
    fillRect(fb, 24, 165, 1, 64, 4);
    fillRect(fb, 359, 165, 1, 64, 4);
    drawCenteredLabel(fb, 22, "MPEG Real-Time Seamless playback", 2);
    drawCenteredLabel(fb, 44, "This tech demo plays the map theme of", 2);
    drawCenteredLabel(fb, 54, "the Amiga game Stardust with only 76kB", 2);
    drawCenteredLabel(fb, 64, "of MPEG Audio data which are reordered", 2);
    drawCenteredLabel(fb, 74, "in real-time", 2);
    drawCenteredLabel(fb, 94, "This could be an interesting loading", 2);
    drawCenteredLabel(fb, 104, "theme for a game that is not taking", 2);
    drawCenteredLabel(fb, 114, "up much space.", 2);
    drawLabel(fb, 149, 184, "MPEG SECTOR MAP", 11);
    drawStaticMap(fb);
}

void draw2x2(unsigned char *fb, int x, int y, int color) {
    register u_char *dst = fb + y * SCREEN_WIDTH + x;
    dst[0] = color;
    dst[1] = color;
    dst += SCREEN_WIDTH;
    dst[0] = color;
    dst[1] = color;
}

void drawRectangle(unsigned char *fb, int x, int y, int w, int h, int color) {
    int i;

#if 0
	/* Horizontal lines */
	drawLine(fb + y * SCREEN_WIDTH + x, w, color);
	drawLine(fb + (y + h - 1) * SCREEN_WIDTH + x, w, color);

	/* Vertical lines */
	for (i = 0; i < h; i++)
	{
		fb[(y + i) * SCREEN_WIDTH + x] = color;
		fb[(y + i) * SCREEN_WIDTH + x + w - 1] = color;
	}
#else
    register u_char *top = fb + y * SCREEN_WIDTH + x;
    register u_char *bottom = top + h * SCREEN_WIDTH;
    top[0] = color;
    top[w] = color;
    bottom[0] = color;
    bottom[w] = color;
#endif
}

void createVideoBuffers() {
    int x;

    paVideo1 = (u_char *)srqcmem(VBUFFER_SIZE, VIDEO1);
    paVideo2 = (u_char *)srqcmem(VBUFFER_SIZE, VIDEO2);

    fillVideoBuffer(paVideo1, 0);
    fillVideoBuffer(paVideo2, 0);
    /* Plane A is transparent dynamic ink; Plane B contains the static map. */
    drawPanel(paVideo2);

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

void copyRect(sourceBuffer, targetBuffer, x, y, width, height, sourceWidth) u_char *sourceBuffer,
    *targetBuffer;
u_short x, y, width, height, sourceWidth;
{
    register u_char *dst = targetBuffer + y * SCREEN_WIDTH + x;
    register u_char *src = sourceBuffer;
    register u_short h, w;
    register u_char tmp;

    for (h = 0; h < height; h++) {
        for (w = 0; w < width; w++) {
            tmp = *src++;
            if (tmp) {
                *dst = tmp;
            }
            dst++;
        }
        dst += SCREEN_WIDTH - width;
        src += sourceWidth - width;
    }
}

void clearRect(videoBuffer, x, y, width, height, color) u_char *videoBuffer;
u_short x, y, width, height;
u_char color;
{
    register u_int value = (color << 24) | (color << 16) | (color << 8) | color;
    register u_int *dst = (u_int *)(videoBuffer + y * SCREEN_WIDTH + x);
    register u_short h, w;

    width >>= 2;

    for (h = 0; h < height; h++) {
        for (w = 0; w < width; w++)
            *dst++ = value;
        dst += (SCREEN_WIDTH >> 2) - width;
    }
}

void initGraphics() { createVideoBuffers(); }

void graphicsStartSong() {
    int playhead;

    songTicks = 0;
    firstPass = 1;
    /* The two planes are prepared completely before the decoder starts. */
    fillVideoBuffer(paVideo1, 0);
    drawPanel(paVideo2);
    playhead = MAP_X;
    drawPlayhead(paVideo1, playhead);
    lastPlayhead = playhead;
    glow1 = glow2 = glow3 = -1;
    /* The song begins with a beat.  This changes CLUT entry 7, not pixels. */
    mapLabelPhase = 0;
    videoSetMapLabelFlash(0);
}

static int partBeatPhase(partTicks, partLength) unsigned long partTicks;
unsigned long partLength;
{
    unsigned long middle = partLength / 2;
    unsigned long beatAge;

    if (partTicks < BEAT_PULSE_TICKS)
        beatAge = partTicks;
    else if (partTicks >= middle && partTicks < middle + BEAT_PULSE_TICKS)
        beatAge = partTicks - middle;
    else
        return -1;
    return (int)(beatAge / BEAT_PHASE_TICKS);
}

static int mapLabelBeatPhase() {
    unsigned long partTicks;
    int remainingLoops;

    if (firstPass && songTicks < INTRO_TICKS)
        return partBeatPhase(songTicks, INTRO_TICKS);

    partTicks = firstPass ? songTicks - INTRO_TICKS : songTicks;
    if (partTicks < RESTART_TICKS)
        return partBeatPhase(partTicks, RESTART_TICKS);

    partTicks -= RESTART_TICKS;
    /* Seven independent loop files follow restart.  Their equal duration
     * lets us find the current file without division on every audio update. */
    remainingLoops = 7;
    while (partTicks >= LOOP_TICKS && --remainingLoops)
        partTicks -= LOOP_TICKS;
    return partBeatPhase(partTicks, LOOP_TICKS);
}

void graphicsAudioUpdate() {
    unsigned long total;
    unsigned long positionTicks;
    unsigned long finalFrameStart;
    int sector;
    int playhead;
    int labelPhase;

    /* MA_TRIG_UPD is raised for each decoded 44.1 kHz Layer-II frame.
     * A frame is 1152 samples: 1152 * 90000 / 44100 = 2351 ticks. */
    songTicks += MPEG_FRAME_TICKS;
    if (firstPass && songTicks >= FIRST_PASS_TICKS) {
        songTicks -= FIRST_PASS_TICKS;
        firstPass = 0;
    }
    if (!firstPass && songTicks >= REPEAT_TICKS)
        songTicks %= REPEAT_TICKS;

    labelPhase = mapLabelBeatPhase();
    if (labelPhase != mapLabelPhase) {
        mapLabelPhase = labelPhase;
        videoSetMapLabelFlash(labelPhase);
    }

    total = firstPass ? FIRST_PASS_TICKS : REPEAT_TICKS;
    /* The last update arrives one MPEG frame before the following loop starts.
     * Map that complete final frame interval to the last sector column. */
    finalFrameStart = total - MPEG_FRAME_TICKS;
    positionTicks = songTicks;
    if (positionTicks > finalFrameStart)
        positionTicks = finalFrameStart;
    if (firstPass)
        sector = (int)((positionTicks * (SECTOR_COUNT - 1)) / finalFrameStart);
    else
        /* The repeating pass starts at sector 1: the intro column stays unused. */
        sector = 1 + (int)((positionTicks * (SECTOR_COUNT - 2)) / finalFrameStart);
    playhead = MAP_X + sector * SECTOR_PITCH;
    if (playhead != lastPlayhead) {
        if (glow3 >= 0)
            erasePlayhead(paVideo1, glow3);
        if (glow2 >= 0)
            drawAfterglow(paVideo1, glow2, 10);
        if (glow1 >= 0)
            drawAfterglow(paVideo1, glow1, 9);
        if (lastPlayhead >= 0)
            drawAfterglow(paVideo1, lastPlayhead, 8);

        glow3 = glow2;
        glow2 = glow1;
        glow1 = lastPlayhead;
        drawPlayhead(paVideo1, playhead);
        lastPlayhead = playhead;
    }
}
