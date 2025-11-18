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
/* #define ENABLE_VIDEO */
#define ENABLE_AUDIO

extern int errno;

#define DEBUG(c)                         \
	if ((c) == -1)                       \
	{                                    \
		printf("FAIL: c (%d)\n", errno); \
	}

int mpegStatus;
int maPath, mvPath, maMapId, mvMapId;

static int mpegFile = -1;
static int collected_buffers = 0;

static PCB mpegPcb;
static PCL mvPcl[1];
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
	printf("status %x\n", FMA_STATUS);

	maPath = open(devName, 0); /* Open MPEG Audio Device */
	free(devName);			   /* Release memory */
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
	mpegPcb.PCB_Rec = 200000; /* assume that there is only 1 EOR */
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

	printf("InitMPEG %d %d - %X %X - %X %X\n", maPath, mvPath, &mvPcl[0], mvPcl[0].PCL_Nxt, &mvPcl[MV_PCL_COUNT - 1], mvPcl[MV_PCL_COUNT - 1].PCL_Nxt);
}

unsigned long *fmadrv_static = 0;
unsigned long *fdrvs1_static = 0;
int CalcLba(int m, int s, int f)
{
	return f + s * 75 + m * 60 * 75;
}

void FindFmvDriverStruct()
{
	int i;
	unsigned long *ptr;
	unsigned long dma_adr;
	unsigned long fma_dclk_adr;

	if (fdrvs1_static != 0)
		return;

	/* This is very dirty ! But it seems to work !*/
	ptr = (unsigned long *)(0x001500);
	ptr = (unsigned long *)ptr[0x48 / 4];
	ptr += (mvPath - 1); /* not sure about this */
	ptr = (unsigned long *)ptr[0];
	ptr = (unsigned long *)ptr[1];
	fdrvs1_static = (unsigned long *)ptr[1];
	/* On MiSTer it is 0x00dfb180 */
	/* On cdiemu with vmpega.rom it is also 0x00dfb180 */
	/* On 210/05 with VMPEG it is 0x00dfa980 */
	printf("fdrvs1_static: %x\n", fdrvs1_static);
	dma_adr = fdrvs1_static[83];
	fma_dclk_adr = fdrvs1_static[85];
	printf("dma_adr %x\n", dma_adr);		   /* must be e04000 */
	printf("fma_dclk_adr %x\n", fma_dclk_adr); /* must be e03010 */
	/* confirm the correctness of fdrvs1_static */
	DEBUG(dma_adr == 0xe04000);
	DEBUG(fma_dclk_adr == 0xe03010);

	for (i = 0; i < 100; i++)
	{
		/* if (temp_array1[i] != temp_array2[i]) */
		{
			/* printf("Diff at %d %x %x\n",i,temp_array1[i], temp_array2[i]); */
		}
	}
}

void FindFmaDriverStruct()
{
	int i;

	if (fmadrv_static != 0)
		return;
	for (i = 0x00dfa000; i < 0x00dfc000; i += 4)
	{
		if ((*(unsigned long *)i) == 0xe0300)
		{
			printf("Found fmadriv at %x\n", i);
		}
	}
}

void playMpeg()
{
	int channel = 0;
	int streamid = 0;
	int m, s, f, lba;
	int i = 0;
	mpegStatus = MPP_STOP;
	printf("status %x\n", FMA_STATUS);

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
	printf("mvPath %x\n", mvPath);

	FindFmvDriverStruct();
	FindFmaDriverStruct();

	DEBUG(mv_pos(mvPath, mvMapId, 0, 0, 0));
	DEBUG(mv_window(mvPath, mvMapId, 0, 0, 768, 560, 0));

#ifdef ENABLE_AUDIO
	/* LtoL=LOUD: LtoR=MUTE: RtoR=LOUD: RtoL=MUTE */
	ma_cntrl(maPath, maMapId, 0x00800080, streamid);
	DEBUG(ma_trigger(maPath, MA_SIG_BASE | 0x1f));
#endif

	/* Init PCL, PCB */
	initMpegPcls();
	initMpegPcb(channel);

	mpegStatus = MPP_INIT;
	printf("status %x\n", FMA_STATUS);
	/* Setup MPEG Playback */
#ifdef ENABLE_VIDEO
	DEBUG(mv_cdplay(mvPath, mvMapId, MV_SPEED_NORMAL, MV_NO_OFFSET, mvPcl, &mvStatus, MV_NO_SYNC, 0));
#endif

#ifdef ENABLE_AUDIO
	DEBUG(ma_cdplay(maPath, maMapId, MV_NO_OFFSET, maPcl, &maStatus, MV_NO_SYNC, 0));
#endif
	printf("status %x\n", FMA_STATUS);

	/* Assume we are not running from serial stub first */
	mpegFile = open("/cd/VIDEO01.RTF", _READ);
	if (mpegFile < 0)
	{
		/* We are running via serial stub on real hardware and Top Gun Disc? */
		printf("Serial stub?\n");
		mpegFile = open("/cd/MUSICS/edmusics.rtf", _READ);
	}
	DEBUG(mpegFile >= 0);

	lseek(mpegFile, CalcLba(0, 12, 0) * 2048, 0); /* Seek to beginning */
	DEBUG(ss_play(mpegFile, &mpegPcb));
	printf("status %x\n", FMA_STATUS);
	printf("Started Play %d at %x at DCLK %x\n", mpegFile, CDIC_TIME, FMA_DCLK);
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
		unsigned int status = FMA_STATUS;

		/* printf("PCB %x %x %x %x\n", mpegPcb.PCB_Stat, mpegPcb.PCB_Sig, maStatus.asy_stat, status); */
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
		/*
		if (full_cnt > 90)
			printf("MA %x %d %d\n", mpegPcb.PCB_Stat, full_cnt, err_cnt);
		*/
	}
	else if ((sigCode & 0xf000) == MA_SIG_BASE)
	{
		/* Event coming from MPEG Audio driver */
		static unsigned long last_dclk = 0;
		static int last_ma_dsc = 0;
		MA_status maInfo;
		MAmapDesc *audiomap;
		unsigned long dclk = FMA_DCLK;
		int ma_dsc_diff;
		unsigned long dclk_diff;
		unsigned short *regs = ((unsigned short *)0x0E03000);
		static int cd_is_paused = 0;
		unsigned int status = FMA_STATUS;

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

		/* DEBUG(ma_status(maPath, &maInfo)); */
		/* audiomap = ma_info(maPath, maMapId);*/
		if (full_cnt >= 100 && !cd_is_paused)
		{
			/* printf("pause!\n"); */
			DEBUG(ss_pause(mpegFile));
			cd_is_paused = 1;
		}

		if (full_cnt <= 50 && cd_is_paused)
		{
			/* printf("cont!\n"); */
			DEBUG(ss_cont(mpegFile));
			cd_is_paused = 0;
		}
#if 0
		/*printf("MA %x %x %x %x %x %x\n", sigCode,
			   audiomap->MD_EnLoop,
			   audiomap->MD_StLoop,
			   audiomap->MD_LpCnt,
			   audiomap->MD_LCntr,
			   maInfo.MAS_CurAdr);*/
#else
		/* printf("MA %x %d %x\n", sigCode, full_cnt, status); */
#endif

		/*
		printf("MA %x %d %x %x %x %x\n", sigCode, maInfo.MAS_Stream, FMA_CMD, FMA_R02, FMA_RUN, FMA_IER);
		*/
#if 0
		for (i = 0; i < 18; i++)
		{
			printf("%x ", regs[i]);
		}

		printf("\n");
#endif

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
		int pic_rate = FMV_PIC_RATE;
		int disp_rate = FMV_DISP_RATE;
		int frame_rate = FMV_FRAME_RATE;
		int dts = FMV_DTS;
		int fmv_dclk = FMV_DCLK;
		int fma_dclk = FMA_DCLK;
		int gen_sync_diff = FMV_GEN_SYNC_DIFF;
		int V_Stat = *(unsigned short *)(((char *)fdrvs1_static) + 0x134);
		int V_BufStat = *(unsigned char *)(((char *)fdrvs1_static) + 0x17b);
		int V_CurDelta = *(unsigned long *)(((char *)fdrvs1_static) + 0x104);
		int V_NewDelta = *(unsigned long *)(((char *)fdrvs1_static) + 0x108);
		int V_SCRupd = *(unsigned long *)(((char *)fdrvs1_static) + 0x166);
		int V_PICCnt = *(unsigned char *)(((char *)fdrvs1_static) + 0x1cd);
		int V_SCR = *(unsigned long *)(((char *)fdrvs1_static) + 0xca);
		int V_DataSize = *(unsigned long *)(((char *)fdrvs1_static) + 0x126);

		int V_LastSCR = *(unsigned long *)(((char *)fdrvs1_static) + 0x15c);
		int V_DTSVal = *(unsigned short *)(((char *)fdrvs1_static) + 0x1c0);

		if (sigCode & MV_TRIG_PIC)
		{
			static int reduce_print_cnt = 0;
			/* FMV_DCLK = FMV_DCLK - 2; */

			reduce_print_cnt++;
			/* if ((reduce_print_cnt & 0x7) == 0) */
			{
				unsigned long timecode = FMV_TIMECD;
				unsigned long h = (timecode >> 6) & 0x1f;		 /* 5 bits for hours*/
				unsigned long m = (timecode) & 0x3f;			 /* 6 bits for minutes*/
				unsigned long p = (timecode >> 16) & 0x3f;		 /* 6 bits for picture*/
				unsigned long s = (timecode >> (16 + 6)) & 0x3f; /* 6 bits for seconds */
				MVmapDesc *desc = mv_info(mvPath, mvMapId);

				printf("%08x %08x %02d:%02d:%02d.%d\n", FMV_TIMECD, desc->MD_TimeCd, h, m, s, p);
#if 0
				printf("%d %d %d %d %d %d %d %d %d %d\n", dts, fmv_dclk, V_SCR, V_PICCnt, V_CurDelta, V_Stat, V_BufStat, V_DataSize, V_DTSVal, V_LastSCR );
				printf("%d %d %d %d %d\n", V_LastSCR, V_SCR, V_LastSCR - V_SCR, fmv_dclk, gen_sync_diff);
				printf("%d %d %d\n", (fma_dclk>>6), dts, (fma_dclk>>6) - dts);
#endif
			}

			if (mpegStatus == MPP_INIT)
				mpegPic();
		}
	}
}
