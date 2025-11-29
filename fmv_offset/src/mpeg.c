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
#include "cross_mpg.h"

/* Have at least one of them enabled! */
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

void initMpegPcb(channel) int channel;
{
	int i;

	for (i = 0; i < 32; i++)
	{
		mvCil[i] = (PCL *)NULL;
	}

	for (i = 0; i < 16; i++)
	{
		maCil[i] = (PCL *)NULL;
	}

	mvCil[channel] = mvPcl;
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
	mpegPcb.PCB_Rec = 1; /* assume that there is only 1 EOR */
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

	initMpegPcls();
	initMpegPcb(0);

	printf("InitMPEG %d %d - %X %X - %X %X\n", maPath, mvPath, &mvPcl[0], mvPcl[0].PCL_Nxt, &mvPcl[MV_PCL_COUNT - 1], mvPcl[MV_PCL_COUNT - 1].PCL_Nxt);
}

void playMpeg(path, channel) char *path;
int channel;
{
	int mv_host_size;
	mpegStatus = MPP_STOP;

	/* Create FMV maps */
	mvMapId = mv_create(mvPath, PLAYHOST);
	maMapId = ma_create(maPath, PLAYHOST);

	mvDesc = (MVmapDesc *)mv_info(mvPath, mvMapId);
	printf("playMpeg %s %d - %d %d\n", path, channel, maMapId, mvMapId);
	/* Setup initial FMV parameters */
	DEBUG(mv_trigger(mvPath, MV_TRIG_MASK));
	DEBUG(mv_selstrm(mvPath, mvMapId, 0, 768, 560, 25));
	DEBUG(mv_borcol(mvPath, mvMapId, 0, 0, 0));
	DEBUG(mv_org(mvPath, mvMapId, 0, 0));
	DEBUG(mv_pos(mvPath, mvMapId, 0, 0, 0));
	DEBUG(mv_window(mvPath, mvMapId, 0, 0, 768, 560, 0));
	DEBUG(mv_show(mvPath, 0));

#ifdef ENABLE_AUDIO
	/* LtoL=LOUD: LtoR=MUTE: RtoR=LOUD: RtoL=MUTE */
	ma_cntrl(maPath, maMapId, 0x00800080, 0L);
	DEBUG(ma_trigger(maPath, MA_SIG_BASE | 0x1f));
#endif

	/* Init PCL, PCB */
	initMpegPcls();
	initMpegPcb(channel);

	mpegStatus = MPP_INIT;

	/* Setup MPEG Playback */
#ifdef ENABLE_VIDEO
	DEBUG(mv_hostplay(mvPath, mvMapId, MV_SPEED_NORMAL, cross_mpg_len, cross_mpg, 0, &mvStatus, MV_NO_SYNC, 0));
	/* DEBUG(mv_hostnext(mvPath, mvMapId)); */ /* Not sure if this is needed */
#endif

#ifdef ENABLE_AUDIO
	DEBUG(ma_cdplay(maPath, maMapId, MV_NO_OFFSET, maPcl, &maStatus, MV_NO_SYNC, 0));
#endif

	printf("Started Play %s at %x\n", path, cross_mpg);
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

	printf("PIC: %d %d - %d %d  %x %x %x\n", width, height, offsetX, offsetY, FMV_IMGRT, FMV_SYS_PRPA, FMV_RC6);

	DEBUG(mv_pos(mvPath, mvMapId, 0, 0, 0));
	DEBUG(mv_window(mvPath, mvMapId, 0, 0, 768, 560, 0));

	DEBUG(mv_show(mvPath, 0));

	mpegStatus = MPP_PLAY;
}

int sigcnt = 0;

int mpegSignal(sigCode)
int sigCode;
{
	if (sigCode == MPEG_SIG_PCB)
	{
		/* Occurs when playback has finished */
		printf("PCB %x %x %x\n", mpegPcb.PCB_Stat, mpegPcb.PCB_Sig, maStatus.asy_stat);
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
	}
	else if ((sigCode & 0xf000) == MV_SIG_BASE)
	{
		printf("MV %x\n", sigCode);

		/* Event coming from MPEG Video driver */
		mpegPic();
	}
}

/*
Expected output
MV b200   0 3000230 0 0 c800c8
PIC: 200 200 - 284 180
MV b180   0 c800c8 11c00b4 0 c800c8
PIC: 200 200 - 284 180
MV b100   0 c800c8 11c00b4 0 c800c8
PIC: 200 200 - 284 180
MV b010   0 c800c8 11c00b4 0 c800c8
PIC: 200 200 - 284 180
MV2 391

On MiSTer

Video: LI="625": 0
InitMPEG 4 5 - D0004A D00066 - D00862 D0004A
Starting FMV
playMpeg /cd/VIDEO01.RTF 0 - 1 1
Started Play /cd/VIDEO01.RTF at d01376
MV b080   0 30001e0 0 0 30001e0
PIC 30001e0 30001e0 0 0 0
MV b010   0 30001e0 0 0 30001e0
PIC 30001e0 30001e0 0 0 0
MV2 91

*/