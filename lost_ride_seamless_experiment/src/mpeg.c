#include <sysio.h>
#include <ucm.h>
#include <stdio.h>
#include <memory.h>
#include <cdfm.h>
#include <csd.h>
#include <mv.h>
#include <ma.h>

#include "mpeg.h"
#include "hwreg.h"
#include "sfx.c"

/* Have at least one of them enabled! */
#define ENABLE_AUDIO
/* #define ENABLE_VIDEO */

extern int errno;

#define DEBUG(c)                         \
	if ((c) == -1)                       \
	{                                    \
		printf("FAIL: c (%d)\n", errno); \
	}

int mpegStatus;
int maPath, mvPath, maMapId, mvMapId;

static int mpegFile = -1;

static PCB mpegPcb;
static PCL maPcl[MA_PCL_COUNT];
static PCL *mvCil[32];
static PCL *maCil[16];

static STAT_BLK mvStatus;
static STAT_BLK maStatus;

static MVmapDesc *mvDesc;

char *mpegDataBuffer;

void initMpegAudio()
{
	char *devName = csd_devname(DT_MPEGA, 1); /* Get MPEG Audio Device Name */
	maPath = open(devName, 0);                /* Open MPEG Audio Device */
	free(devName);                            /* Release memory */
}

void initMpegVideo()
{
	char *devName = csd_devname(DT_MPEGV, 1); /* Get MPEG Video Device Name */
	mvPath = open(devName, 0);                /* Open MPEG Video Device */
	free(devName);                            /* Release memory */
}
/*
 XX XX XX XX  32 31 30 XX
 29 28 27 26  25 24 23 22
 21 20 19 18  17 16 15 XX
 14 13 12 11  10 09 08 07
 06 05 04 03  02 01 00 XX
*/

unsigned long long get_scr(unsigned char *buf)
{
	unsigned long scr = 0;

	scr = ((unsigned long long)(buf[4] & 0x0E)) << 29;
	scr |= ((unsigned long long)buf[5]) << 22;
	scr |= ((unsigned long long)(buf[6] & 0xFE)) << 14;
	scr |= ((unsigned long long)buf[7]) << 7;
	scr |= ((unsigned long long)(buf[8] & 0xFE)) >> 1;

	return scr;
}

void set_scr(unsigned char *buf, unsigned long long scr)
{
	buf[4] = 0x21 | ((scr >> 29) & 0x0E); /* '01', SCR[32..30], marker */
	buf[5] = (scr >> 22) & 0xFF;
	buf[6] = 0x01 | ((scr >> 14) & 0xFE); /* SCR[21..15], marker */
	buf[7] = (scr >> 7) & 0xFF;
	buf[8] = 0x01 | ((scr << 1) & 0xFE); /* SCR[6..0], marker */
}

void initMpegPcb(channel) int channel;
{
	int i;

	for (i = 0; i < 16; i++)
	{
		maCil[i] = (PCL *)NULL;
	}

	maCil[channel] = maPcl;

	mpegPcb.PCB_Video = NULL;
	mpegPcb.PCB_Audio = NULL;
#ifdef ENABLE_VIDEO
	mpegPcb.PCB_Video = mvCil;
#endif
#ifdef ENABLE_AUDIO
	mpegPcb.PCB_Audio = maCil;
#endif
	mpegPcb.PCB_Data = NULL;
	mpegPcb.PCB_Sig = MPEG_SIG_PCB;
	mpegPcb.PCB_Chan = 0x00000001 << channel;
	mpegPcb.PCB_AChan = 0;
	mpegPcb.PCB_Rec = 10000; /* assume that there is only 1 EOR */
	mpegPcb.PCB_Stat = 0;

	mvStatus.asy_stat = 0;
	mvStatus.asy_sig = MV_SIG_STAT;

	maStatus.asy_stat = 0;
	maStatus.asy_sig = MA_SIG_STAT;

	mpegStatus = MPP_STOP;
}

void initMpegPcl(pcl, sig, next, buffer, length)
	PCL *pcl; /* pointer to the  PCL to initialise */
short sig;    /* signal to be sent on buffer full */
PCL *next;    /* pointer to next PCL */
char *buffer; /* pointer to data buffer */
int length;   /* buffer size in number of sectors */
{
	pcl->PCL_Sig = sig;
	pcl->PCL_Nxt = next;
	pcl->PCL_Buf = buffer;
	pcl->PCL_BufSz = length;
	pcl->PCL_Ctrl = 0x01;
	pcl->PCL_Err = NULL;
	pcl->PCL_Cnt = 0;
}

int current_pcl = 0;

unsigned long long inc_table[MA_PCL_COUNT];

void initMpegPcls()
{
	int i;

	for (i = 0; i < MA_PCL_COUNT; i++)
	{
		initMpegPcl(
			&(maPcl[i]),
			MA_SIG_PCL,
			&(maPcl[(i + 1) % MA_PCL_COUNT]),
			&mpegDataBuffer[i * MPEG_SECTOR_SIZE],
			1);

		inc_table[i] = get_scr(&mpegDataBuffer[i * MPEG_SECTOR_SIZE]) - get_scr(&mpegDataBuffer[(i - 1) * MPEG_SECTOR_SIZE]);
		/*
		printf("SCR %lu %lu\n", get_scr(&mpegDataBuffer[i * MPEG_SECTOR_SIZE]),
			   get_scr(&mpegDataBuffer[(i + 1) * MPEG_SECTOR_SIZE]) - get_scr(&mpegDataBuffer[i * MPEG_SECTOR_SIZE]));
			*/
	}

	/* TODO not yet fully understood */
	inc_table[0] = 6700; /* 35 sec until dead */
	inc_table[0] = 6750; /* 35 sec until dead */
	inc_table[0] = 6450; /* 34 sec until dead */
	inc_table[0] = 6900; /* 26 sec until UNF */
	inc_table[0] = 3000; /* 5 sec until dead */
	inc_table[0] = 4000; /* 6 sec until dead */
	inc_table[0] = 5000; /* 11 sec until dead */
	inc_table[0] = 6800; /* 33 sec until UNF */
	inc_table[0] = 6770; /* 33 sec until dead */

	for (i = 0; i < MA_PCL_COUNT; i++)
	{
		printf("SCR %lu %lu %lu\n", get_scr(&mpegDataBuffer[i * MPEG_SECTOR_SIZE]),
			   get_scr(&mpegDataBuffer[(i + 1) * MPEG_SECTOR_SIZE]) - get_scr(&mpegDataBuffer[i * MPEG_SECTOR_SIZE]),
			   inc_table[i]);
	}
}

void initMpeg()
{
	initMpegAudio();
	initMpegVideo();

	mpegDataBuffer = (char *)srqcmem((MA_PCL_COUNT)*MPEG_SECTOR_SIZE, SYSRAM);
	memcpy(mpegDataBuffer, &lostride_mpg[MPEG_SECTOR_SIZE * 2], MPEG_SECTOR_SIZE * MA_PCL_COUNT);

	initMpegPcls();
	initMpegPcb(0);
}

void playMpeg()
{
	int channel = 0;

	mpegStatus = MPP_STOP;

	/* Create FMV maps */
	mvMapId = mv_create(mvPath, PLAYCD);
	maMapId = ma_create(maPath, PLAYCD);

	mvDesc = (MVmapDesc *)mv_info(mvPath, mvMapId);
	printf("playMpeg %d - %d %d\n", channel, maMapId, mvMapId);
	/* Setup initial FMV parameters */
	DEBUG(mv_trigger(mvPath, MV_TRIG_MASK));
	DEBUG(mv_selstrm(mvPath, mvMapId, 0, 768, 560, 25));
	DEBUG(mv_borcol(mvPath, mvMapId, 0, 0, 0));
	DEBUG(mv_org(mvPath, mvMapId, 0, 0));
	DEBUG(mv_pos(mvPath, mvMapId, 0, 0, 0));
	DEBUG(mv_window(mvPath, mvMapId, 0, 0, 768, 560, 0));

#ifdef ENABLE_AUDIO
	/* LtoL=LOUD: LtoR=MUTE: RtoR=LOUD: RtoL=MUTE */
	ma_cntrl(maPath, maMapId, 0x00800080, 0L);
	DEBUG(ma_trigger(maPath, MA_SIG_BASE | 0x1f));
#endif

	/* Init PCL, PCB */
	initMpegPcb(channel);

	mpegStatus = MPP_INIT;

	/* Setup MPEG Playback */
#ifdef ENABLE_VIDEO
	DEBUG(mv_cdplay(mvPath, mvMapId, MV_SPEED_NORMAL, MV_NO_OFFSET, mvPcl, &mvStatus, MV_NO_SYNC, 0));
#endif

#ifdef ENABLE_AUDIO
	DEBUG(ma_cdplay(maPath, maMapId, MV_NO_OFFSET, maPcl, &maStatus, MV_NO_SYNC, 0));
#endif

	/* Open file, start play */
	printf("Started Play\n");
}

void stopMpeg()
{
	if (mpegStatus == MPP_STOP)
		return;

	DEBUG(mv_abort(mvPath));
#ifdef ENABLE_AUDIO
	DEBUG(ma_abort(maPath));
#endif

	DEBUG(mv_hide(mvPath));
	DEBUG(mv_release(mvPath));

#ifdef ENABLE_AUDIO
	DEBUG(ma_cntrl(maPath, maMapId, 0x80808080, 0L));
	DEBUG(ma_release(maPath));
#endif

	close(mpegFile);
	mpegFile = -1;

	printf("Play Stopped\n");

	mpegStatus = MPP_STOP;
}

void mpegPic()
{
	int width, height, offsetX, offsetY;
	MA_status maInfo;
	mvDesc = mv_info(mvPath, mvMapId);

	/* Extract Image Size */
	width = mvDesc->MD_ImgSz;
	height = width & 0x0000FFFF;
	width = (width >> 16) & 0x0000FFFF;

	offsetX = (768 - width) / 2;
	offsetY = (560 - height) / 2;

	DEBUG(mv_show(mvPath, 0));

	mpegStatus = MPP_PLAY;
}

int sigcnt = 0;
int repeat = 0;

int mpegSignal(sigCode)
int sigCode;
{
	if (sigCode == MPEG_SIG_PCB)
	{
		/* Occurs when playback has finished */
		printf("PCB %x %x %x\n", mpegPcb.PCB_Stat, mpegPcb.PCB_Sig, maStatus.asy_stat);

		if (!repeat)
		{
			repeat = 1;
			stopMpeg();
		}
	}
	else if (sigCode == MA_SIG_STAT)
	{
		printf("MA2 %x\n", maStatus.asy_stat);
	}
	else if (sigCode == MV_SIG_STAT)
	{
		printf("MV2 %x\n", mvStatus.asy_stat);
	}
	else if (sigCode == MV_SIG_PCL)
	{
	}
	else if (sigCode == MA_SIG_PCL)
	{
	}
	else if ((sigCode & 0xf000) == MA_SIG_BASE)
	{
		/* Event coming from MPEG Audio driver */
		static unsigned long last_dclk = 0;
		static int last_ma_dsc = 0;
		MA_status maInfo;
		unsigned long dclk = FMA_DCLK;
		int ma_dsc_diff;
		unsigned long dclk_diff;
		unsigned short status = FMA_STATUS;

		DEBUG(ma_status(maPath, &maInfo));

		ma_dsc_diff = maInfo.MAS_DSC - last_ma_dsc;
		dclk_diff = dclk - last_dclk;

		if (sigCode & MA_TRIG_UNF)
			printf("MA %d %x %x\n", dclk_diff, sigCode, status);

		while (maPcl[current_pcl].PCL_Ctrl != 1)
		{
			unsigned char *buf_last;
			unsigned char *buf_current;

			int last_pcl = current_pcl - 1;
			if (last_pcl < 0)
				last_pcl += MA_PCL_COUNT;

			buf_last = &mpegDataBuffer[last_pcl * MPEG_SECTOR_SIZE];
			buf_current = &mpegDataBuffer[current_pcl * MPEG_SECTOR_SIZE];

			set_scr(buf_current, get_scr(buf_last) + inc_table[current_pcl]);

			initMpegPcl(
				&(maPcl[current_pcl]),
				MA_SIG_PCL,
				&(maPcl[(current_pcl + 1) % MA_PCL_COUNT]),
				buf_current,
				1);

			current_pcl = (current_pcl + 1) % MA_PCL_COUNT;
		}

		last_dclk = dclk;
		last_ma_dsc = maInfo.MAS_DSC;
		sigcnt++;
	}
	else if ((sigCode & 0xf000) == MV_SIG_BASE)
	{
		/* Event coming from MPEG Video driver */
		if (sigCode & MV_TRIG_PIC)
		{
			if (mpegStatus == MPP_INIT)
				mpegPic();
		}
	}
}
