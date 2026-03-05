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
#include "cross_audio.h"
#include "cross_video.h"
#include "graphics.h"

/* Have at least one of them enabled! */
#define ENABLE_VIDEO
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

static PCB mpegPcb;

static STAT_BLK mvStatus;
static STAT_BLK maStatus;

static MVmapDesc *mvDesc;

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

	mpegPcb.PCB_Video = NULL;
	mpegPcb.PCB_Audio = NULL;
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

void initMpeg()
{
	initMpegAudio();
	initMpegVideo();

	initMpegPcb(0);
}

unsigned long *fdrvs1_static = 0;

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
}

void StartSyncedPlayback()
{

#ifdef ENABLE_AUDIO
	/*DEBUG(ma_loop(maPath, maMapId, 0, cross_audio_mpg_len, 10000));*/
	DEBUG(ma_hostplay(maPath, maMapId, cross_audio_mpg_len, cross_audio_mpg, 0, &maStatus, -2, 0));
	DEBUG(ma_cntrl(maPath, maMapId, 0x00800080, 0L));
#endif

#ifdef ENABLE_VIDEO
	/* Without mv_loop, the decoder will stop and we can't scroll through the picture */
	/* DEBUG(mv_loop(mvPath, mvMapId, 0, cross_video_mpg_len, 10000)); */
	DEBUG(mv_hostplay(mvPath, mvMapId, MV_SPEED_NORMAL, cross_video_mpg_len, cross_video_mpg, 0, &mvStatus, maPath, 9900));
#endif
	printf("Started Play\n");
}

void playMpeg(path, channel) char *path;
int channel;
{
	int i;

	int mv_host_size;
	int V_DTSFnd;
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
	DEBUG(mv_pos(mvPath, mvMapId, 768 / 2, 560 / 2 - 128, 0));
	DEBUG(mv_window(mvPath, mvMapId, 0, 0, 768, 560, 0));
	DEBUG(mv_show(mvPath, 0));

	FindFmvDriverStruct();

#ifdef ENABLE_AUDIO
	/* LtoL=LOUD: LtoR=MUTE: RtoR=LOUD: RtoL=MUTE */
	ma_cntrl(maPath, maMapId, 0x00800080, 0L);
	DEBUG(ma_trigger(maPath, MA_SIG_BASE | 0x1f));
#endif

	/* Init PCL, PCB */
	initMpegPcb(channel);

	mpegStatus = MPP_INIT;
	StartSyncedPlayback();
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

MotionStatus mvstat;

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

	DEBUG(mv_status(mvPath, &mvstat));
	DEBUG(mv_show(mvPath, 0));

	mpegStatus = MPP_PLAY;
}

int sigcnt = 0;

static unsigned long regdump[200 * 3][20];
static int regdump_index = 0;

void print_registers()
{
	int i, j;

	for (i = 0; i < regdump_index; i++)
	{
		printf("%3d ", i);
		for (j = 0; j <= 7; j++)
		{
			printf(" %08x", regdump[i][j]);
		}

		printf("\n");
	}
}

static unsigned long last_dclk = 0;

unsigned short fma_sigcodebuf[8];
unsigned short fma_sigcodebuf_wrpos = 0;
unsigned short fma_sigcodebuf_rdpos = 0;

unsigned short fmv_sigcodebuf[8];
unsigned short fmv_sigcodebuf_wrpos = 0;
unsigned short fmv_sigcodebuf_rdpos = 0;

int mpegSignal(sigCode)
int sigCode;
{
	static int finished_playback_blank_cnt = 0;

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
		finished_playback_blank_cnt = 10;
	}
	else if (sigCode == MV_SIG_PCL)
	{
	}
	else if (sigCode == MA_SIG_PCL)
	{
	}
	else if ((sigCode & 0xf000) == MA_SIG_BASE)
	{
		/* printf("MA %x\n", sigCode); */
		if (sigCode & MA_TRIG_DEC)
		{
			fma_sigcodebuf[fma_sigcodebuf_wrpos] = sigCode;
			fma_sigcodebuf_wrpos = (fma_sigcodebuf_wrpos + 1) & 7;
		}
	}
	else if ((sigCode & 0xf000) == MV_SIG_BASE)
	{
		if (sigCode & (MV_TRIG_BUF | MV_TRIG_LPD | MV_TRIG_NIS | MV_TRIG_PIC))
		{
			fmv_sigcodebuf[fmv_sigcodebuf_wrpos] = sigCode;
			fmv_sigcodebuf_wrpos = (fmv_sigcodebuf_wrpos + 1) & 7;
		}

		if (mpegStatus == MPP_INIT)
			mpegPic();
	}
	else if (sigCode == SIG_BLANK)
	{
		if (finished_playback_blank_cnt)
		{
			finished_playback_blank_cnt--;
			if (!finished_playback_blank_cnt)
				print_registers();
		}
		dc_ssig(videoPath, SIG_BLANK, 0);
	}
}

void poll_state()
{
	if (fdrvs1_static)
	{
		int V_BufStat = *(unsigned char *)(((char *)fdrvs1_static) + 0x17b);
		unsigned long dclk = FMA_DCLK;
		unsigned short pics = FMV_PICS_IN_FIFO;
		unsigned short dts = FMV_DTS;
		unsigned long imgsz = FMV_IMGSZ;
		unsigned long picsz = FMV_PICSZ;

		static unsigned short last_pics;
		static unsigned long last_dts;
		static unsigned long last_picsz;
		static unsigned long last_imgsz;
		static unsigned long last_bufstat;

		static int reset_after_event = 0;
		unsigned long dclkdiff = dclk - last_dclk;

		if ((dts != last_dts) ||
			(pics != last_pics) ||
			(last_bufstat != V_BufStat) ||
			(last_picsz != picsz) ||
			(last_imgsz != imgsz) ||
			(fma_sigcodebuf_wrpos != fma_sigcodebuf_rdpos) ||
			(fmv_sigcodebuf_wrpos != fmv_sigcodebuf_rdpos) ||
			(reset_after_event && dclkdiff > 850))
		{
			regdump[regdump_index][0] = dts;
			regdump[regdump_index][1] = pics;
			regdump[regdump_index][2] = V_BufStat;
			regdump[regdump_index][3] = (fma_sigcodebuf_rdpos == fma_sigcodebuf_wrpos) ? 0 : fma_sigcodebuf[fma_sigcodebuf_rdpos];
			regdump[regdump_index][4] = (fmv_sigcodebuf_rdpos == fmv_sigcodebuf_wrpos) ? 0 : fmv_sigcodebuf[fmv_sigcodebuf_rdpos];
			regdump[regdump_index][5] = imgsz;
			regdump[regdump_index][6] = picsz;
			regdump[regdump_index][7] = dclkdiff;

			regdump_index++;

			last_dts = dts;
			last_pics = pics;
			last_bufstat = V_BufStat;
			last_dclk = dclk;
			last_picsz = picsz;
			last_imgsz = imgsz;

			reset_after_event = 0;

			if (fma_sigcodebuf_wrpos != fma_sigcodebuf_rdpos)
			{
				fma_sigcodebuf_rdpos = (fma_sigcodebuf_rdpos + 1) & 7;
				reset_after_event = 1;
			}
			if (fmv_sigcodebuf_wrpos != fmv_sigcodebuf_rdpos)
			{
				fmv_sigcodebuf_rdpos = (fmv_sigcodebuf_rdpos + 1) & 7;
				reset_after_event = 1;
			}
		}
	}
}
