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
		mvCil[i] = mvPcl;
	}

	for (i = 0; i < 16; i++)
	{
		maCil[i] = maPcl;
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
	mpegStatus = MPP_STOP;

	/* Create FMV maps */
	mvMapId = mv_create(mvPath, PLAYCD);
	maMapId = ma_create(maPath, PLAYCD);

	mvDesc = (MVmapDesc *)mv_info(mvPath, mvMapId);
	printf("playMpeg %s %d - %d %d\n", path, channel, maMapId, mvMapId);
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
	mpegFile = open(path, _READ);
	lseek(mpegFile, 0, 0); /* Seek to beginning */
	DEBUG(ss_play(mpegFile, &mpegPcb));
	printf("Started Play %s %d\n", path, mpegFile);
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

	printf("PIC: %d %d - %d %d\n", width, height, offsetX, offsetY);

	DEBUG(mv_pos(mvPath, mvMapId, offsetX, offsetY, 0));
	DEBUG(mv_window(mvPath, mvMapId, 0, 0, width, height, 0));
	DEBUG(mv_show(mvPath, 0));

#ifdef ENABLE_AUDIO
	/* Setup volume */
	DEBUG(ma_status(maPath, &maInfo));

	printf("AUDIO: %X\n", maInfo.MAS_Head);
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
		/* Check for buffers */
		int full_cnt = 0;
		int err_cnt = 0;
		int i;
		for (i = 0; i < MV_PCL_COUNT; i++)
		{
			if (mvPcl[i].PCL_Ctrl & 0x01)
			{
				full_cnt++;
			}
			if (mvPcl[i].PCL_Ctrl & 0x80)
			{
				err_cnt++;
			}
		}
		/* Buffers should never fill. Report via console if it happens */
		if (full_cnt > 4)
			printf("MV %x %d %d\n", mpegPcb.PCB_Stat, full_cnt, err_cnt);
		/*
		if (full_cnt == 1)
		{
			#define CDIC_TIME (*((unsigned long *)0x303C02))
			printf("TIME %lx\n", CDIC_TIME);
		}*/
	}
	else if (sigCode == MA_SIG_PCL)
	{
		/* Check for buffers */
		int full_cnt = 0;
		int err_cnt = 0;
		int i;
		for (i = 0; i < MA_PCL_COUNT; i++)
		{
			if (maPcl[i].PCL_Ctrl & 0x01)
			{
				full_cnt++;
			}
			if (maPcl[i].PCL_Ctrl & 0x80)
			{
				err_cnt++;
			}
		}

		if (full_cnt > 4)
			printf("MA %x %d %d\n", mpegPcb.PCB_Stat, full_cnt, err_cnt);
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

		DEBUG(ma_status(maPath, &maInfo));

		ma_dsc_diff = maInfo.MAS_DSC - last_ma_dsc;
		dclk_diff = dclk - last_dclk;

		last_dclk = dclk;
		last_ma_dsc = maInfo.MAS_DSC;
		sigcnt++;
	}
	else if ((sigCode & 0xf000) == MV_SIG_BASE)
	{
		/* Event coming from MPEG Video driver */
		static unsigned long first_dclk = 0;
		int pics = FMV_PICS_IN_FIFO;

		int fma_dclk = FMA_DCLK >> 6;
		int fmv_dts = FMV_DTS;
		int relative_dclk;
		static int reduce_print_cnt = 0;

		if (!first_dclk)
			first_dclk = fma_dclk;
		relative_dclk = fma_dclk - first_dclk;

		reduce_print_cnt++;
		if ((reduce_print_cnt & 0xf) == 0)
			printf("%d\n", relative_dclk, fmv_dts, relative_dclk - fmv_dts);

		if (sigCode & MV_TRIG_PIC)
		{
			if (mpegStatus == MPP_INIT)
				mpegPic();
		}
	}
	else if ((sigCode & 0xf000) == MA_SIG_BASE)
	{
		/* Event coming from MPEG Video driver */
		static unsigned int wired_or = 0;

		wired_or |= sigCode;
		printf("A %x %x\n", sigCode, wired_or);
	}
}
