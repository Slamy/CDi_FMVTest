#include <sysio.h>
#include <ucm.h>
#include <stdio.h>
#include <memory.h>
#include <cdfm.h>
#include <csd.h>
#include <mv.h>
#include <ma.h>

#include "mpeg.h"

extern int errno;

#define DEBUG(c) if((c)==-1) { printf("FAIL: c (%d)\n", errno); }

int mpegStatus;
int maPath, mvPath, maMapId, mvMapId;

static int mpegFile = -1;

static PCB mpegPcb;
static PCL mvPcl[MV_PCL_COUNT];
static PCL maPcl[MA_PCL_COUNT];
static PCL *mvCil[32];
static PCL *maCil[16];

static STAT_BLK  mvStatus;
static STAT_BLK  maStatus;

static MVmapDesc *mvDesc;

char *mpegDataBuffer;

void initMpegAudio() {
	char *devName  = csd_devname(DT_MPEGA, 1); /* Get MPEG Audio Device Name */
	maPath = open(devName, 0);                 /* Open MPEG Audio Device */
	free(devName);                             /* Release memory */
}

void initMpegVideo() {
	char *devName  = csd_devname(DT_MPEGV, 1); /* Get MPEG Video Device Name */
	mvPath = open(devName, 0);                 /* Open MPEG Video Device */
	free(devName);                             /* Release memory */
}

void initMpegPcb(channel)
	int channel;
{
	int i;

	for (i = 0; i < 32; i++) {
		mvCil[i] = (PCL *)NULL;
	}

	for (i = 0; i < 16; i++) {
		maCil[i] = (PCL *)NULL;
	}

	mvCil[channel] = mvPcl;
	maCil[channel] = maPcl; 

	mpegPcb.PCB_Video = mvCil;
	mpegPcb.PCB_Audio = maCil;
	mpegPcb.PCB_Data  = NULL;
	mpegPcb.PCB_Sig   = MPEG_SIG_PCB;
	mpegPcb.PCB_Chan  = 0x00000001 << channel;
	mpegPcb.PCB_AChan = 0;
	mpegPcb.PCB_Rec   = 1;		/* assume that there is only 1 EOR */
	mpegPcb.PCB_Stat  = 0;

	mvStatus.asy_stat = 0;
  	mvStatus.asy_sig = MV_SIG_STAT;

  	maStatus.asy_stat = 0;
  	maStatus.asy_sig = MA_SIG_STAT;

	mpegStatus = MPP_STOP;
}

void initMpegPcl(pcl, sig, next, buffer, length)
	PCL *pcl;     /* pointer to the  PCL to initialise */
	short sig;    /* signal to be sent on buffer full */
	PCL *next;    /* pointer to next PCL */
	char *buffer; /* pointer to data buffer */
	int length;   /* buffer size in number of sectors */
{
	pcl->PCL_Sig   = sig;
	pcl->PCL_Nxt   = next;
	pcl->PCL_Buf   = buffer;
	pcl->PCL_BufSz = length;
	pcl->PCL_Ctrl  = 0; 	
	pcl->PCL_Err   = NULL;
	pcl->PCL_Cnt   = 0;
}

void initMpegPcls() {
	char* address = mpegDataBuffer;
	int i;

	for (i = 0; i < MV_PCL_COUNT; i++) {
		initMpegPcl(
			&(mvPcl[i]),
			MV_SIG_PCL,
			&(mvPcl[(i + 1) % MV_PCL_COUNT]),
			address,
			1
		);
		address += MPEG_SECTOR_SIZE;
	}
	
	for (i = 0; i < MA_PCL_COUNT; i++) {
		initMpegPcl(
			&(maPcl[i]),
			MA_SIG_PCL,
			&(maPcl[(i + 1) % MA_PCL_COUNT]),
			address,
			1
		);
		address += MPEG_SECTOR_SIZE;
	}
}

void initMpeg() {
	initMpegAudio();
	initMpegVideo();

	mpegDataBuffer = (char*)srqcmem((MV_PCL_COUNT + MA_PCL_COUNT) * MPEG_SECTOR_SIZE, SYSRAM);

	initMpegPcls();
	initMpegPcb(0);

	printf("InitMPEG %d %d - %X %X - %X %X\n", maPath, mvPath, &mvPcl[0], mvPcl[0].PCL_Nxt, &mvPcl[MV_PCL_COUNT - 1], mvPcl[MV_PCL_COUNT - 1].PCL_Nxt);
}

void playMpeg(path, channel)
	char *path;
	int channel;
{
	mpegStatus = MPP_STOP;

	/* Create FMV maps */
	mvMapId = mv_create(mvPath, PLAYCD);
	maMapId = ma_create(maPath, PLAYCD);

  	mvDesc = (MVmapDesc *) mv_info(mvPath, mvMapId);
	printf("playMpeg %s %d - %d %d\n", path, channel, maMapId, mvMapId);
	/* Setup initial FMV parameters */
	DEBUG(mv_trigger(mvPath, MV_TRIG_MASK));
	DEBUG(mv_selstrm(mvPath, mvMapId, 0, 768, 560, 25));
	DEBUG(mv_borcol(mvPath, mvMapId, 0, 0, 0));
	DEBUG(mv_org(mvPath, mvMapId, 0, 0));
	DEBUG(mv_pos(mvPath, mvMapId, 0, 0, 0));
	DEBUG(mv_window(mvPath, mvMapId, 0, 0, 768, 560, 0));
	
	
	/* LtoL=MUTE: LtoR=MUTE: RtoR=MUTE: RtoL=MUTE */
  	DEBUG(ma_cntrl(maPath, maMapId, 0x80808080, 0L));

	/* Init PCL, PCB */
	initMpegPcls();
	initMpegPcb(channel);

	mpegStatus = MPP_INIT;

	/* Setup MPEG Playback */
	DEBUG(mv_cdplay(mvPath, mvMapId, MV_SPEED_NORMAL, MV_NO_OFFSET, mvPcl, &mvStatus, MV_NO_SYNC, 0));
	DEBUG(ma_cdplay(maPath, maMapId, MV_NO_OFFSET, maPcl, &maStatus, MV_NO_SYNC, 0));

	/* Open file, start play */
	mpegFile = open(path, _READ);
	lseek(mpegFile, 0, 0);		/* Seek to beginning */
	DEBUG(ss_play(mpegFile, &mpegPcb));
	printf("Started Play %s %d\n", path, mpegFile);
}

void stopMpeg()
{
	if (mpegStatus == MPP_STOP) return;

	DEBUG(mv_abort(mvPath));
	DEBUG(ma_abort(maPath));

	DEBUG(mv_hide(mvPath));
	DEBUG(mv_release(mvPath));

	DEBUG(ma_cntrl(maPath, maMapId, 0x80808080, 0L));
	DEBUG(ma_release(maPath));

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
  	width  = mvDesc->MD_ImgSz;
  	height = width & 0x0000FFFF;
  	width  = (width >> 16) & 0x0000FFFF;

  	offsetX = (768-width)/2;
    offsetY = (560-height)/2;

	printf("PIC: %d %d - %d %d\n", width, height, offsetX, offsetY);

	DEBUG(mv_pos(mvPath, mvMapId, offsetX, offsetY, 0));
	DEBUG(mv_window(mvPath, mvMapId, 0, 0, width, height, 0));
	DEBUG(mv_show(mvPath, 0));

	/* Setup volume */
	DEBUG(ma_status(maPath, &maInfo));

	printf("AUDIO: %X\n", maInfo.MAS_Head);
	if ((maInfo.MAS_Head & MA_AUD_MODE) == MA_AUD_STEREO) {
		/* LtoL=LOUD: LtoR=MUTE: RtoR=LOUD: RtoL=MUTE */
		ma_cntrl(maPath, maMapId, 0x00800080, 0L);
	}
	else {
		/* LtoL=MUTE: LtoR=MUTE: RtoR=LOUD: RtoL=LOUD */
		ma_cntrl(maPath, maMapId, 0x80800000, 0L);
	}

	mpegStatus = MPP_PLAY;
}

int mpegSignal(sigCode)
	int sigCode;
{
	if (sigCode > MV_SIG_BASE) {
		if (sigCode & MV_TRIG_PIC) {
			if (mpegStatus == MPP_INIT) mpegPic();
		}
	}
	else {
		switch(sigCode) {
			case MPEG_SIG_PCB:
				stopMpeg();
			break;
		}
	}
}