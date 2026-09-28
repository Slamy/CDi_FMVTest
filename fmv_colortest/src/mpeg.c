#include <cdfm.h>
#include <csd.h>
#include <ma.h>
#include <memory.h>
#include <mv.h>
#include <stdio.h>
#include <sysio.h>
#include <ucm.h>

#include "cross_mpg.h"
#include "graphics.h"
#include "hwreg.h"
#include "mpeg.h"
#include "video.h"

extern int errno;

#define DEBUG(c)                                                                                                       \
    if ((c) == -1) {                                                                                                   \
        printf("FAIL: c (%d)\n", errno);                                                                               \
    }

int mpegStatus;
int mvPath, mvMapId;

static int mpegFile = -1;

static STAT_BLK mvStatus;
static STAT_BLK maStatus;

static MVmapDesc *mvDesc;

char *mpegDataBuffer;

void initMpegVideo() {
    char *devName = csd_devname(DT_MPEGV, 1); /* Get MPEG Video Device Name */
    mvPath = open(devName, 0);                /* Open MPEG Video Device */
    free(devName);                            /* Release memory */
}

void initMpegPcb() {
    mvStatus.asy_stat = 0;
    mvStatus.asy_sig = MV_SIG_STAT;

    mpegStatus = MPP_STOP;
}

void initMpeg() {
    initMpegVideo();
    initMpegPcb(0);
}

void playMpeg() {
    int mv_host_size;
    int V_DTSFnd;
    mpegStatus = MPP_STOP;

    /* Create FMV maps */
    mvMapId = mv_create(mvPath, PLAYHOST);

    /* Setup initial FMV parameters */
    DEBUG(mv_trigger(mvPath, MV_TRIG_MASK));
    DEBUG(mv_selstrm(mvPath, mvMapId, 0, 768, 560, 25));
    DEBUG(mv_borcol(mvPath, mvMapId, 0, 0, 0));
    DEBUG(mv_org(mvPath, mvMapId, 0, 0));
    DEBUG(mv_pos(mvPath, mvMapId, -100, 200, 0));
    DEBUG(mv_window(mvPath, mvMapId, 0, 0, 768, 560, 0));
    DEBUG(mv_show(mvPath, 0));

    mpegStatus = MPP_INIT;

    /* Setup MPEG Playback */
    DEBUG(mv_hostplay(mvPath, mvMapId, MV_SPEED_NORMAL, cross_mpg_len, cross_mpg, 0, &mvStatus, MV_NO_SYNC, 0));

    printf("Started Play\n");
}

void mpegPic() {
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

int mpegSignal(sigCode)
int sigCode;
{
    if (sigCode == MA_SIG_STAT) {
        printf("MA2 %x\n", maStatus.asy_stat);
    } else if (sigCode == MV_SIG_STAT) {
        printf("MV2 %x\n", mvStatus.asy_stat);
    } else if (sigCode == MV_SIG_PCL) {
    } else if (sigCode == MA_SIG_PCL) {
    } else if ((sigCode & 0xf000) == MA_SIG_BASE) {
    } else if ((sigCode & 0xf000) == MV_SIG_BASE) {
        printf("MV %x\n", sigCode);

        /* Event coming from MPEG Video driver */
        if (mpegStatus == MPP_INIT)
            mpegPic();
    } else if (sigCode == SIG_BLANK) {
        dc_ssig(videoPath, SIG_BLANK, 0);
    }
}
