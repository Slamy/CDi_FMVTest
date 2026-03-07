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

void initMpegPcb(channel) int channel;
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
	if (!mpegDataBuffer)
		exit(0);

	initMpegPcls();
	initMpegPcb(0);

	printf("InitMPEG %d %d - %X %X - %X %X\n", maPath, mvPath, &mvPcl[0], mvPcl[0].PCL_Nxt, &mvPcl[MV_PCL_COUNT - 1], mvPcl[MV_PCL_COUNT - 1].PCL_Nxt);
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

void playMpeg()
{
	int channel = 0;
	int streamid = 0;
	int m, s, f, lba;
	int i = 0;
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
	DEBUG(mv_show(mvPath, 0));

	FindFmvDriverStruct();

#ifdef ENABLE_AUDIO
	/* LtoL=LOUD: LtoR=MUTE: RtoR=LOUD: RtoL=MUTE */
	ma_cntrl(maPath, maMapId, 0x00800080, streamid);
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

	/* Assume we are not running from serial stub first */
	mpegFile = open("/cd/SPACEACE.RTF", _READ);
	if (mpegFile < 0)
	{
		/* We are running via serial stub on real hardware and Top Gun Disc? */
		printf("Serial stub?\n");
		mpegFile = open("/cd/MPEGAV/AVSEQ01.DAT", _READ);
	}
	DEBUG(mpegFile >= 0);

	DEBUG(lseek(mpegFile, 0, 0));
	DEBUG(ss_play(mpegFile, &mpegPcb));
	printf("Started Play %d\n", mpegFile);
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

static unsigned long regdump[200 * 3][20];
static int regdump_index = 0;
static int recording_stopped = 0;

void print_registers()
{
	int i, j;
	recording_stopped = 1;

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
		print_registers();
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
		/* printf("MA %x\n", sigCode); */
		if (sigCode & MA_TRIG_DEC)
		{
			fma_sigcodebuf[fma_sigcodebuf_wrpos] = sigCode;
			fma_sigcodebuf_wrpos = (fma_sigcodebuf_wrpos + 1) & 7;
		}
	}
	else if ((sigCode & 0xf000) == MV_SIG_BASE)
	{
		/* if (sigCode & (MV_TRIG_BUF | MV_TRIG_LPD | MV_TRIG_NIS | MV_TRIG_PIC)) */
		{
			fmv_sigcodebuf[fmv_sigcodebuf_wrpos] = sigCode;
			fmv_sigcodebuf_wrpos = (fmv_sigcodebuf_wrpos + 1) & 7;
		}

		if (sigCode & MV_TRIG_PIC)
		{
			static int piccnt = 0;

			if (mpegStatus == MPP_INIT)
				mpegPic();

			piccnt++;
			if (piccnt == 80)
			{
				print_registers();
			}
		}
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
	if (regdump_index > 500 || recording_stopped)
		return;

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
