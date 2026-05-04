#include <cdfm.h>
#include <csd.h>
#include <ma.h>
#include <memory.h>
#include <mv.h>
#include <stdio.h>
#include <sysio.h>
#include <ucm.h>

#include "hwreg.h"
#include "mpeg.h"
#include "video.h"

#include "graphics.h"

/* Have at least one of them enabled! */
#define ENABLE_AUDIO
#define ENABLE_VIDEO
/* #define HOSTPLAY */
/* #define DO_PAUSE */
#define DO_SLOWMO
/* #define PRINT_REGISTERS */

#ifdef HOSTPLAY
#include "cross_audio.h"
#include "cross_video.h"
#endif

extern int errno;

#define DEBUG(c)                                                               \
    if ((c) == -1) {                                                           \
        printf("FAIL: c (%d)\n", errno);                                       \
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
static MAmapDesc *maDesc;

char *mpegDataBuffer;

void initMpegAudio() {
    char *devName = csd_devname(DT_MPEGA, 1); /* Get MPEG Audio Device Name */
    maPath = open(devName, 0);                /* Open MPEG Audio Device */
    free(devName);                            /* Release memory */
}

void initMpegVideo() {
    char *devName = csd_devname(DT_MPEGV, 1); /* Get MPEG Video Device Name */
    mvPath = open(devName, 0);                /* Open MPEG Video Device */
    free(devName);                            /* Release memory */
}

void initMpegPcb(channel) int channel;
{
    int i;

    for (i = 0; i < 32; i++) {
        mvCil[i] = (PCL *)mvPcl;
    }

    for (i = 0; i < 16; i++) {
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

void initMpegPcl(pcl, sig, next, buffer,
                 length) PCL *pcl; /* pointer to the  PCL to initialise */
short sig;                         /* signal to be sent on buffer full */
PCL *next;                         /* pointer to next PCL */
char *buffer;                      /* pointer to data buffer */
int length;                        /* buffer size in number of sectors */
{
    pcl->PCL_Sig = sig;
    pcl->PCL_Nxt = next;
    pcl->PCL_Buf = buffer;
    pcl->PCL_BufSz = length;
    pcl->PCL_Ctrl = 0;
    pcl->PCL_Err = NULL;
    pcl->PCL_Cnt = 0;
}

void initMpegPcls() {
    char *address = mpegDataBuffer;
    int i;

    for (i = 0; i < MV_PCL_COUNT; i++) {
        initMpegPcl(&(mvPcl[i]), MV_SIG_PCL, &(mvPcl[(i + 1) % MV_PCL_COUNT]),
                    address, 1);
        address += MPEG_SECTOR_SIZE;
    }

    for (i = 0; i < MA_PCL_COUNT; i++) {
        initMpegPcl(&(maPcl[i]), MA_SIG_PCL, &(maPcl[(i + 1) % MA_PCL_COUNT]),
                    address, 1);
        address += MPEG_SECTOR_SIZE;
    }
}

void initMpeg() {
    initMpegAudio();
    initMpegVideo();

    mpegDataBuffer = (char *)srqcmem(
        (MV_PCL_COUNT + MA_PCL_COUNT) * MPEG_SECTOR_SIZE, SYSRAM);
    if (!mpegDataBuffer)
        exit(0);

    initMpegPcls();
    initMpegPcb(0);
}

unsigned long *fdrvs1_static = 0;

void FindFmvDriverStruct() {
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
    printf("dma_adr %x\n", dma_adr);           /* must be e04000 */
    printf("fma_dclk_adr %x\n", fma_dclk_adr); /* must be e03010 */
    /* confirm the correctness of fdrvs1_static */
    DEBUG(dma_adr == 0xe04000);
    DEBUG(fma_dclk_adr == 0xe03010);
}

void playMpeg() {
    int channel = 0;
    int streamid = 0;
    int m, s, f, lba;
    int i = 0;
    mpegStatus = MPP_STOP;

    /* Create FMV maps */
#ifdef HOSTPLAY
    mvMapId = mv_create(mvPath, PLAYHOST);
    maMapId = ma_create(maPath, PLAYHOST);
#else
    mvMapId = mv_create(mvPath, PLAYCD);
    maMapId = ma_create(maPath, PLAYCD);
#endif

    mvDesc = (MVmapDesc *)mv_info(mvPath, mvMapId);
    maDesc = (MAmapDesc *)ma_info(maPath, maMapId);

    printf("playMpeg %d - %d %d\n", channel, maMapId, mvMapId);
    /* Setup initial FMV parameters */
    DEBUG(mv_trigger(mvPath, MV_TRIG_MASK));
    DEBUG(mv_selstrm(mvPath, mvMapId, 0, 768, 560, 25));
    DEBUG(mv_borcol(mvPath, mvMapId, 0, 0, 0));
    DEBUG(mv_org(mvPath, mvMapId, 0, 0));
#ifdef HOSTPLAY
    DEBUG(mv_pos(mvPath, mvMapId, 768 / 2, 560 / 2 - 128, 0));
#else
    DEBUG(mv_pos(mvPath, mvMapId, 0, 0, 0));
#endif
    DEBUG(mv_window(mvPath, mvMapId, 0, 0, 768, 560, 0));
    DEBUG(mv_show(mvPath, 0));

    FindFmvDriverStruct();

#ifdef ENABLE_AUDIO
    /* LtoL=LOUD: LtoR=MUTE: RtoR=LOUD: RtoL=MUTE */
    ma_cntrl(maPath, maMapId, 0x00800080, 0L);
    DEBUG(ma_trigger(maPath, MA_SIG_BASE | 0x1f));
#endif

    /* Init PCL, PCB */
    initMpegPcls();
    initMpegPcb(channel);

    mpegStatus = MPP_INIT;

#ifdef HOSTPLAY

#ifdef ENABLE_AUDIO
    /*DEBUG(ma_loop(maPath, maMapId, 0, cross_audio_mpg_len, 10000));*/
    DEBUG(ma_hostplay(maPath, maMapId, cross_audio_mpg_len, cross_audio_mpg, 0,
                      &maStatus, MV_NO_SYNC, 0));
    DEBUG(ma_cntrl(maPath, maMapId, 0x00800080, 0L));
#endif

#ifdef ENABLE_VIDEO
    /* Without mv_loop, the decoder will stop and we can't scroll through the
     * picture */
    /* DEBUG(mv_loop(mvPath, mvMapId, 0, cross_video_mpg_len, 10000)); */
    DEBUG(mv_hostplay(mvPath, mvMapId, MV_SPEED_NORMAL, cross_video_mpg_len,
                      cross_video_mpg, 0, &mvStatus, MV_NO_SYNC, 0));
#endif
    printf("Started Play\n");

#else
    /* Setup MPEG Playback */
#ifdef ENABLE_VIDEO
    DEBUG(mv_cdplay(mvPath, mvMapId, MV_SPEED_NORMAL, MV_NO_OFFSET, mvPcl,
                    &mvStatus, -2, 0));
#endif
#ifdef ENABLE_AUDIO
    DEBUG(
        ma_cdplay(maPath, maMapId, MV_NO_OFFSET, maPcl, &maStatus, mvPath, 0));
#endif

    /* Assume we are not running from serial stub first */
    mpegFile = open("/cd/VIDEO01.RTF", _READ);
    DEBUG(mpegFile >= 0);

    DEBUG(lseek(mpegFile, 0, 0));
    DEBUG(ss_play(mpegFile, &mpegPcb));
    printf("Started Play %d\n", mpegFile);
#endif
}

void stopMpeg() {
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

    DEBUG(mv_pos(mvPath, mvMapId, offsetX, offsetY, 0));
    DEBUG(mv_window(mvPath, mvMapId, 0, 0, width, height, 0));
    DEBUG(mv_show(mvPath, 0));

    mpegStatus = MPP_PLAY;
}

int sigcnt = 0;

static unsigned long regdump[500][25];
static int regdump_index = 0;
static int recording_stopped = 0;
/* clang-format off */
static char regsize[]={
	32,8,8,16,16,
	32,32,32,32,16,
	8,16,32,32,16,
	16,32,32,32,16,
	16,8,16,16,

	/* Timestamp */
	32
};
/* clang-format on */

void print_registers() {
    int i, j;

    if (recording_stopped)
        return;
    recording_stopped = 1;

#ifdef PRINT_REGISTERS
    for (i = 0; i < regdump_index; i++) {
        printf("%3d ", i);
        for (j = 0; j <= 24; j++) {
            switch (regsize[j]) {
            case 8:
                printf(" %02x", regdump[i][j]);
                break;
            case 16:
                printf(" %04x", regdump[i][j]);
                break;
            case 32:
                printf(" %08x", regdump[i][j]);
                break;
            }
        }

        printf("\n");
    }
#endif
}

static unsigned long last_dclk = 0;

unsigned short fma_sigcodebuf[8];
unsigned short fma_sigcodebuf_wrpos = 0;
unsigned short fma_sigcodebuf_rdpos = 0;

unsigned short fmv_sigcodebuf[8];
unsigned short fmv_sigcodebuf_wrpos = 0;
unsigned short fmv_sigcodebuf_rdpos = 0;

int do_pause = 0;
static int piccnt = 0;

int mpegSignal(sigCode)
int sigCode;
{
    static int finished_playback_blank_cnt = 0;
    static int restart_playback_blank_cnt = 0;
    MotionStatus mvstat;
    MA_status mastat;

    if (sigCode == MPEG_SIG_PCB) {
        /* Occurs when playback has finished */
        printf("PCB %x %x %x\n", mpegPcb.PCB_Stat, mpegPcb.PCB_Sig,
               maStatus.asy_stat);
        print_registers();
    } else if (sigCode == MA_SIG_STAT) {
        printf("MA2 %x\n", maStatus.asy_stat);
    } else if (sigCode == MV_SIG_STAT) {
        printf("MV2 %x\n", mvStatus.asy_stat);
#ifdef HOSTPLAY
        finished_playback_blank_cnt = 10;
#endif
    } else if (sigCode == MV_SIG_PCL) {
    } else if (sigCode == MA_SIG_PCL) {
    } else if ((sigCode & 0xf000) == MA_SIG_BASE) {
        /* printf("MA %x\n", sigCode); */
        /* if (sigCode & (MA_TRIG_DEC | MA_TRIG_UNF | MA_TRIG_EOI)) */
        {
            fma_sigcodebuf[fma_sigcodebuf_wrpos] = sigCode;
            fma_sigcodebuf_wrpos = (fma_sigcodebuf_wrpos + 1) & 7;
        }
    } else if ((sigCode & 0xf000) == MV_SIG_BASE) {
        /* if (sigCode & (MV_TRIG_BUF | MV_TRIG_LPD | MV_TRIG_NIS |
         * MV_TRIG_PIC)) */
        {
            fmv_sigcodebuf[fmv_sigcodebuf_wrpos] = sigCode;
            fmv_sigcodebuf_wrpos = (fmv_sigcodebuf_wrpos + 1) & 7;
        }

        if (sigCode & MV_TRIG_NIS) {
            DEBUG(mv_status(mvPath, &mvstat));
            printf("NIS %x\n", mvstat.MVS_ImgSz);
        }

        if (sigCode & MV_TRIG_PIC) {
#ifndef PRINT_REGISTERS
            unsigned char full_mv_cnt = 0;
            unsigned char full_ma_cnt = 0;
            int i;
            for (i = 0; i < MV_PCL_COUNT; i++) {
                if (mvPcl[i].PCL_Ctrl & 0x01) {
                    full_mv_cnt++;
                }
            }
            for (i = 0; i < MA_PCL_COUNT; i++) {
                if (mvPcl[i].PCL_Ctrl & 0x01) {
                    full_ma_cnt++;
                }
            }

            DEBUG(mv_status(mvPath, &mvstat));
            DEBUG(ma_status(maPath, &mastat));

            if ((piccnt & 7) == 1) {
                printf("PIC %x %d %d %x\n", sigCode, full_mv_cnt, full_ma_cnt,
                       FMV_SCR);
            }
#endif

            if (mpegStatus == MPP_INIT)
                mpegPic();

            piccnt++;
#ifdef DO_PAUSE
            if (piccnt == 20) {
                do_pause = 1;
                restart_playback_blank_cnt = 20;
            }
            if (piccnt == 40) {
                do_pause = 1;
                restart_playback_blank_cnt = 20;
            }
            if (piccnt == 60) {
                do_pause = 1;
                restart_playback_blank_cnt = 20;
            }
#endif

#ifdef DO_SLOWMO
            if (piccnt == 50) {
                DEBUG(mv_chspeed(mvPath, 3, 0, NULL));
                printf("0\n");
            }

            if (piccnt == 100) {
                DEBUG(mv_chspeed(mvPath, MV_SPEED_NORMAL, 0, NULL));
                printf("1\n");
            }
#endif

#ifndef HOSTPLAY
            if (piccnt == 40) {
                print_registers();
            }
#endif
        }
    } else if (sigCode == SIG_BLANK) {
        if (finished_playback_blank_cnt) {
            finished_playback_blank_cnt--;
            if (!finished_playback_blank_cnt)
                print_registers();
        }
#ifdef DO_PAUSE
        if (restart_playback_blank_cnt) {
            restart_playback_blank_cnt--;
            if (!restart_playback_blank_cnt) {
                DEBUG(ss_cont(mpegFile));
                DEBUG(mv_continue(mvPath, 0));
#ifdef ENABLE_AUDIO
                DEBUG(ma_continue(maPath));
#endif
            }
        }
#endif
        dc_ssig(videoPath, SIG_BLANK, 0);
    }
}

int recording_not_yet_started = 1;

void poll_state() {
    int full_cnt = 0;
    static int cd_is_paused = 0;
    int i;

    if (do_pause) {
        DEBUG(mv_pause(mvPath));
#ifdef ENABLE_AUDIO
        DEBUG(ma_pause(maPath));
#endif
        DEBUG(ss_pause(mpegFile));
        do_pause = 0;
    }

#ifdef DO_SLOWMO
    for (i = 0; i < MV_PCL_COUNT; i++) {
        if (mvPcl[i].PCL_Ctrl & 0x01) {
            full_cnt++;
        }
    }

    if (full_cnt >= 100 && !cd_is_paused) {
        print_registers();
        printf("pause!\n");
        DEBUG(ss_pause(mpegFile));
        cd_is_paused = 1;
    }

    if (full_cnt <= 50 && cd_is_paused) {
        printf("cont!\n");
        DEBUG(ss_cont(mpegFile));
        cd_is_paused = 0;
    }
#endif

    if (recording_not_yet_started) {
        unsigned char V_BufStat =
            *(unsigned char *)(((char *)fdrvs1_static) + 0x17b);
        if (V_BufStat == 0x21) {
            recording_not_yet_started = 0;
        }
    }

    if (regdump_index > 480 || recording_stopped || recording_not_yet_started)
        return;

    if (fdrvs1_static) {
        unsigned char V_BufStat =
            *(unsigned char *)(((char *)fdrvs1_static) + 0x17b);
        unsigned short V_Status =
            *(unsigned short *)(((char *)fdrvs1_static) + 0x136);
        unsigned short V_Stat =
            *(unsigned short *)(((char *)fdrvs1_static) + 0x134);
        unsigned long V_PausedSCR =
            *(unsigned long *)(((char *)fdrvs1_static) + 0x144);
        unsigned long V_SCR =
            *(unsigned long *)(((char *)fdrvs1_static) + 0xca);
        unsigned long V_LastSCR =
            *(unsigned long *)(((char *)fdrvs1_static) + 0x15c);
        unsigned short V_DTSVal =
            *(unsigned short *)(((char *)fdrvs1_static) + 0x1c0);

        unsigned short picrate = FMV_PIC_RATE;
        unsigned long dclk = FMA_DCLK;
        unsigned short pics = FMV_PICS_IN_FIFO;
        unsigned short dts = FMV_DTS;
        unsigned long imgsz = FMV_IMGSZ;
        unsigned long picsz = FMV_PICSZ;
        unsigned short vdi_cmd = FMV_VDI_CMD;
        unsigned long md_imgsz = mvDesc->MD_ImgSz;
        unsigned long md_timecd = mvDesc->MD_TimeCd;
        unsigned short md_tmpref = mvDesc->MD_TmpRef;
        unsigned char md_picrt = mvDesc->MD_PicRt;
        unsigned short tmpref = FMV_TMPREF;
        unsigned long pictimecd = FMV_PICTIMECD;
        unsigned long imgtimecd = FMV_IMGTIMECD;

        static unsigned long last_V_BufStat;
        static unsigned long last_V_Status;
        static unsigned long last_V_Stat;
        static unsigned long last_V_PausedSCR;
        static unsigned long last_V_SCR;
        static unsigned long last_V_LastSCR;
        static unsigned short last_V_DTSVal;

        static unsigned short last_pics;
        static unsigned long last_dts;
        static unsigned long last_picsz;
        static unsigned long last_reg_imgsz;
        static unsigned long last_md_imgsz;
        static unsigned long last_md_timecd;
        static unsigned short last_md_tmpref;
        static unsigned char last_md_picrt;
        static unsigned short last_tmpref;
        static unsigned long last_pictimecd;
        static unsigned long last_imgtimecd;

        static int reset_after_event = 0;
        unsigned long dclkdiff = dclk - last_dclk;

        if ((dts != last_dts) || (pics != last_pics) ||
            (last_V_BufStat != V_BufStat) || (last_V_Status != V_Status) ||
            (last_V_Stat != V_Stat) || (last_picsz != picsz) ||
            (last_reg_imgsz != imgsz) ||
            (fma_sigcodebuf_wrpos != fma_sigcodebuf_rdpos) ||
            (fmv_sigcodebuf_wrpos != fmv_sigcodebuf_rdpos) ||
            (last_md_imgsz != md_imgsz) || (last_md_timecd != md_timecd) ||
            (last_md_tmpref != md_tmpref) || (last_md_picrt != md_picrt) ||
            (last_tmpref != tmpref) || (last_pictimecd != pictimecd) ||
            (last_imgtimecd != imgtimecd) ||
            (last_V_PausedSCR != V_PausedSCR) || (last_V_SCR != V_SCR) ||
            (last_V_LastSCR != V_LastSCR) || (last_V_DTSVal != V_DTSVal) ||
            (reset_after_event && dclkdiff > 850)) {
            unsigned char full_mv_cnt = 0;
            unsigned char full_ma_cnt = 0;
            int i;
            for (i = 0; i < MV_PCL_COUNT; i++) {
                if (mvPcl[i].PCL_Ctrl & 0x01) {
                    full_mv_cnt++;
                }
            }

            regdump[regdump_index][0] = dts;
            regdump[regdump_index][1] = pics;
            regdump[regdump_index][2] = V_BufStat;
            regdump[regdump_index][3] =
                (fma_sigcodebuf_rdpos == fma_sigcodebuf_wrpos)
                    ? 0
                    : fma_sigcodebuf[fma_sigcodebuf_rdpos];
            regdump[regdump_index][4] =
                (fmv_sigcodebuf_rdpos == fmv_sigcodebuf_wrpos)
                    ? 0
                    : fmv_sigcodebuf[fmv_sigcodebuf_rdpos];
            regdump[regdump_index][5] = imgsz;
            regdump[regdump_index][6] = picsz;
            regdump[regdump_index][7] = md_imgsz;
            regdump[regdump_index][8] = md_timecd;
            regdump[regdump_index][9] = md_tmpref;
            regdump[regdump_index][10] = md_picrt;
            regdump[regdump_index][11] = tmpref;
            regdump[regdump_index][12] = pictimecd;
            regdump[regdump_index][13] = imgtimecd;
            regdump[regdump_index][14] = V_Status;
            regdump[regdump_index][15] = V_Stat;
            regdump[regdump_index][16] = V_PausedSCR;
            regdump[regdump_index][17] = V_SCR;
            regdump[regdump_index][18] = V_LastSCR;
            regdump[regdump_index][19] = V_DTSVal;
            regdump[regdump_index][20] = piccnt;
            regdump[regdump_index][21] =
                full_mv_cnt | ((FMV_STS & 0x2000) ? 0x00 : 0x80);
            regdump[regdump_index][22] = vdi_cmd;
            regdump[regdump_index][23] = picrate;

            regdump[regdump_index][24] = dclkdiff;

            regdump_index++;

            last_dts = dts;
            last_pics = pics;

            last_V_BufStat = V_BufStat;
            last_V_Status = V_Status;
            last_V_Stat = V_Stat;

            last_V_PausedSCR = V_PausedSCR;
            last_V_SCR = V_SCR;
            last_V_LastSCR = V_LastSCR;
            last_V_DTSVal = V_DTSVal;

            last_dclk = dclk;
            last_picsz = picsz;
            last_reg_imgsz = imgsz;
            last_md_imgsz = md_imgsz;
            last_md_timecd = md_timecd;
            last_md_tmpref = md_tmpref;
            last_md_picrt = md_picrt;
            last_tmpref = tmpref;
            last_pictimecd = pictimecd;
            last_imgtimecd = imgtimecd;

            reset_after_event = 0;

            if (fma_sigcodebuf_wrpos != fma_sigcodebuf_rdpos) {
                fma_sigcodebuf_rdpos = (fma_sigcodebuf_rdpos + 1) & 7;
                reset_after_event = 1;
            }
            if (fmv_sigcodebuf_wrpos != fmv_sigcodebuf_rdpos) {
                fmv_sigcodebuf_rdpos = (fmv_sigcodebuf_rdpos + 1) & 7;
                reset_after_event = 1;
            }
        }
    }
}
