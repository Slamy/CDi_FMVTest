#include <sysio.h>
#include <ucm.h>
#include <stdio.h>
#include <memory.h>
#include <cdfm.h>
#include <csd.h>
#include <mv.h>
#include <ma.h>

#include "mpeg.h"
#include "video.h"
#include "hwreg.h"

/* Have at least one of them enabled! */
#define ENABLE_AUDIO
#define ENABLE_VIDEO

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
static PCL mvPcl[MV_PCL_COUNT];
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
	maPath = open(devName, 0);				  /* Open MPEG Audio Device */
	free(devName);							  /* Release memory */
}

void initMpegVideo()
{
	char *devName = csd_devname(DT_MPEGV, 1); /* Get MPEG Video Device Name */
	mvPath = open(devName, 0);				  /* Open MPEG Video Device */
	free(devName);							  /* Release memory */
}

void initMpegPcb()
{
	int i;

	for (i = 0; i < 32; i++)
	{
		mvCil[i] = (PCL *)mvPcl;
	}

	for (i = 0; i < 16; i++)
	{
		maCil[i] = (PCL *)maPcl;
	}

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
	mpegPcb.PCB_Chan = 0xffffffff;
	mpegPcb.PCB_AChan = 0;
	mpegPcb.PCB_Rec = 2000; /* assume that there is only 1 EOR */
	mpegPcb.PCB_Stat = 0;

	mvStatus.asy_stat = 0;
	mvStatus.asy_sig = MV_SIG_STAT;

	maStatus.asy_stat = 0;
	maStatus.asy_sig = MA_SIG_STAT;

	mpegStatus = MPP_STOP;
}

void initMpegPcl(pcl, sig, next, buffer, length)
	PCL *pcl; /* pointer to the  PCL to initialise */
short sig;	  /* signal to be sent on buffer full */
PCL *next;	  /* pointer to next PCL */
char *buffer; /* pointer to data buffer */
int length;	  /* buffer size in number of sectors */
{
	pcl->PCL_Sig = sig;
	pcl->PCL_Nxt = next;
	pcl->PCL_Buf = buffer;
	pcl->PCL_BufSz = length;
	pcl->PCL_Ctrl = 0;
	pcl->PCL_Err = NULL;
	pcl->PCL_Cnt = 0;
}

void initMpegPcls()
{
	char *address = mpegDataBuffer;
	int i;

	for (i = 0; i < MV_PCL_COUNT; i++)
	{
		initMpegPcl(
			&(mvPcl[i]),
			MV_SIG_PCL,
			&(mvPcl[(i + 1) % MV_PCL_COUNT]),
			address,
			1);
		address += MPEG_SECTOR_SIZE;
	}

	for (i = 0; i < MA_PCL_COUNT; i++)
	{
		initMpegPcl(
			&(maPcl[i]),
			MA_SIG_PCL,
			&(maPcl[(i + 1) % MA_PCL_COUNT]),
			address,
			1);
		address += MPEG_SECTOR_SIZE;
	}
}

void initMpeg()
{
	initMpegAudio();
	initMpegVideo();

	mpegDataBuffer = (char *)srqcmem((MV_PCL_COUNT + MA_PCL_COUNT) * MPEG_SECTOR_SIZE, SYSRAM);
	if (!mpegDataBuffer)
		exit(0);

	initMpegPcls();
	initMpegPcb(0);
}

void playMpeg()
{
	mpegStatus = MPP_STOP;

	/* Create FMV maps */
	mvMapId = mv_create(mvPath, PLAYCD);
	maMapId = ma_create(maPath, PLAYCD);

	mvDesc = (MVmapDesc *)mv_info(mvPath, mvMapId);

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
	initMpegPcls();
	initMpegPcb();

	mpegStatus = MPP_INIT;

	/* Setup MPEG Playback */
#ifdef ENABLE_VIDEO
	DEBUG(mv_cdplay(mvPath, mvMapId, MV_SPEED_NORMAL, MV_NO_OFFSET, mvPcl, &mvStatus, -2, 0));
#endif

#ifdef ENABLE_AUDIO
	DEBUG(ma_cdplay(maPath, maMapId, MV_NO_OFFSET, maPcl, &maStatus, mvPath, 0));
#endif

	/* Assume we are not running from serial stub first */
	mpegFile = open("/cd/VIDEO01.RTF", _READ);
	if (mpegFile < 0)
	{
		/* We are running via serial stub on real hardware and Top Gun Disc? */
		mpegFile = open("/cd/MPEGAV/AVSEQ01.DAT", _READ);
	}
	DEBUG(mpegFile >= 0);

	DEBUG(lseek(mpegFile, 0, 0));
	DEBUG(ss_play(mpegFile, &mpegPcb));
	printf("Started Play\n");
}

void stopMpeg()
{
	if (mpegStatus == MPP_STOP)
		return;

#ifdef ENABLE_VIDEO
	DEBUG(mv_abort(mvPath));
#endif
#ifdef ENABLE_AUDIO
	DEBUG(ma_abort(maPath));
#endif

#ifdef ENABLE_VIDEO
	DEBUG(mv_hide(mvPath));
#endif

#ifdef ENABLE_AUDIO
	DEBUG(ma_cntrl(maPath, maMapId, 0x80808080, 0L));
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

	/* We assume VCD mode, offsetX must be 0 */
	offsetX = 0;
	offsetY = (screen_height * 2 - height) / 2;

	printf("PIC: %d %d - %d %d %d\n", width, height, offsetX, offsetY, screen_height);

	DEBUG(mv_pos(mvPath, mvMapId, offsetX, offsetY, 0));
	DEBUG(mv_window(mvPath, mvMapId, 0, 0, width, height, 0));
	DEBUG(mv_show(mvPath, 0));

#ifdef ENABLE_AUDIO
	/* Setup volume */
	DEBUG(ma_status(maPath, &maInfo));

	if ((maInfo.MAS_Head & MA_AUD_MODE) == MA_AUD_STEREO)
	{
		/* LtoL=LOUD: LtoR=MUTE: RtoR=LOUD: RtoL=MUTE */
		ma_cntrl(maPath, maMapId, 0x00800080, 0L);
	}
	else
	{
		/* LtoL=MUTE: LtoR=MUTE: RtoR=LOUD: RtoL=LOUD */
		ma_cntrl(maPath, maMapId, 0x80800000, 0L);
	}
#endif

	mpegStatus = MPP_PLAY;
}

int mpegSignal(sigCode)
int sigCode;
{
	if (sigCode == MPEG_SIG_PCB)
	{
		/* Occurs when playback has finished */
#ifdef CONFIG_LOOPED_PLAY
		printf("File has ended. Stop and replay\n");
		stopMpeg();
#else
		printf("File has ended. Cause black screen\n");
		DEBUG(mv_hide(mvPath));
#endif
	}
	else if (sigCode == MA_SIG_STAT)
	{
		printf("MA SIG\n");
	}
	else if (sigCode == MV_SIG_STAT)
	{
		printf("MV SIG\n");
	}
	else if (sigCode == MV_SIG_PCL)
	{
	}
	else if (sigCode == MA_SIG_PCL)
	{
	}
	else if ((sigCode & 0xf000) == MA_SIG_BASE)
	{
	}
	else if ((sigCode & 0xf000) == MV_SIG_BASE)
	{
		/* Event coming from MPEG Video driver */

		if (sigCode & MV_TRIG_BUF)
			printf("BUFFER UNDERFLOW\n");

		if (sigCode & MV_TRIG_LPD)
			printf("LAST PIC\n");

		if (sigCode & MV_TRIG_PIC)
		{
			int pics = FMV_PICS_IN_FIFO;

			if (pics < 3)
			{
				printf("PICs in FIFO: %d\n", pics);
			}

			if (mpegStatus == MPP_INIT)
				mpegPic();
		}
	}
}
