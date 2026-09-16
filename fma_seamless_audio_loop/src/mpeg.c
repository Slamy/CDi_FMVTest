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
#include "sfx1.c"
#include "sfx2.c"

/*
 * This is an audio-only MPEG Program Stream player for CD-i.
 *
 * build.sh produces two independent MP2/Program-Stream assets.  The player
 * starts with loop 1, supplies it four times, then supplies loop 2 four
 * times, and repeats that pattern without stopping the hardware decoder.
 * PCL buffers are reused only after the MPEG driver has consumed them.
 */

extern int errno;

#define DEBUG(c)                                                               \
    if ((c) == -1) {                                                           \
        printf("FAIL: c (%d)\n", errno);                                       \
    }

#define PCL_READY 0x01
#define MPEG_DEBUG_PCLS 0

/* Kept callable while the expensive PCL status trace is disabled. */
static void dump_pcls(tag) char *tag;
{
#if MPEG_DEBUG_PCLS
    printf("MPEG %s cur=%d cycle=%lu\n", tag, currentPcl,
           (unsigned long)streamCycle);
#endif
}

int mpegStatus;
int maPath, maMapId;
int mvPath, mvMapId; /* Kept for the application's public interface. */

static PCB mpegPcb;
static PCL maPcl[MA_PCL_MAX];
static PCL *maCil[16];
static STAT_BLK maStatus;

/* RAM backing store for the PCL ring.  Generated assets remain immutable. */
static char *mpegDataBuffer;
static int maPclCount;
static int currentPcl;             /* next returned PCL to refill */
static unsigned long long streamCycle;   /* number of complete PCL rings */
static unsigned long long timelineTicks; /* monotonic 90 kHz stream time */
static int activeLoop;             /* 0 = sfx1, 1 = sfx2 */

#define LOOP_COUNT 2
#define LOOP_REPETITIONS 4

typedef struct {
    unsigned char *data;          /* immutable bytes from generated sfx*.c */
    unsigned long length;
    unsigned long periodTicks;    /* exact MP2 duration, in 90 kHz ticks */
    int pclCount;                 /* must match between switchable loops */
    unsigned long long scr[MA_PCL_MAX];
    unsigned long long pts[MA_PCL_MAX];
    int scrOffset[MA_PCL_MAX];    /* -1 for a PES-continuation sector */
    int ptsOffset[MA_PCL_MAX];    /* -1 when no new PES starts in sector */
    unsigned long long firstRegularScr;
    unsigned long long normalScrDelta;
    int hasMuxPreroll;
} LoopSource;

static LoopSource loopSource[LOOP_COUNT];

static unsigned long long get_timestamp(p)
unsigned char *p;
{
    unsigned long long value;

    value = ((unsigned long long)((p[0] >> 1) & 0x07)) << 30;
    value |= ((unsigned long long)p[1]) << 22;
    value |= ((unsigned long long)(p[2] & 0xfe)) << 14;
    value |= ((unsigned long long)p[3]) << 7;
    value |= ((unsigned long long)(p[4] & 0xfe)) >> 1;
    return value;
}

static void set_timestamp(p, value) unsigned char *p;
unsigned long long value;
{
    p[0] = (p[0] & 0xf0) | ((value >> 29) & 0x0e) | 0x01;
    p[1] = (value >> 22) & 0xff;
    p[2] = 0x01 | ((value >> 14) & 0xfe);
    p[3] = (value >> 7) & 0xff;
    p[4] = 0x01 | ((value << 1) & 0xfe);
}

static int find_audio_pts(sector)
unsigned char *sector;
{
    int offset;

    /* MPEG-1 PES: 00 00 01 c0, length, then the five-byte PTS. */
    for (offset = 0; offset <= MPEG_SECTOR_SIZE - 11; offset++) {
        if (sector[offset] == 0x00 && sector[offset + 1] == 0x00 &&
            sector[offset + 2] == 0x01 && sector[offset + 3] == 0xc0 &&
            (sector[offset + 6] & 0xf0) == 0x20)
            return offset + 6;
    }
    return -1;
}

static int find_pack_scr(sector)
unsigned char *sector;
{
    int offset;

    for (offset = 0; offset <= MPEG_SECTOR_SIZE - 9; offset++) {
        if (sector[offset] == 0x00 && sector[offset + 1] == 0x00 &&
            sector[offset + 2] == 0x01 && sector[offset + 3] == 0xba)
            return offset + 4;
    }
    return -1;
}

static void print_pcl_timestamps(pcl, buffer) PCL *pcl;
char *buffer;
{
    unsigned char *sector;
    unsigned long long scr;
    unsigned long long pts;
    int scrOffset;
    int ptsOffset;

    sector = (unsigned char *)buffer;
    ptsOffset = find_audio_pts(sector);
    if (ptsOffset < 0) {
        printf("MPEG PCL_INIT loop=%d n=%d cycle=%lu no-pts\n", activeLoop + 1,
               (int)(pcl - maPcl), (unsigned long)streamCycle);
        return;
    }

    pts = get_timestamp(&sector[ptsOffset]);
    scrOffset = find_pack_scr(sector);
    if (scrOffset < 0) {
        printf("MPEG PCL_INIT loop=%d n=%d cycle=%lu scr=none pts=%lx:%08lx\n",
               activeLoop + 1, (int)(pcl - maPcl), (unsigned long)streamCycle,
               (unsigned long)(pts >> 32), (unsigned long)pts);
        return;
    }
    scr = get_timestamp(&sector[scrOffset]);
    printf("MPEG PCL_INIT loop=%d n=%d cycle=%lu scr=%lx:%08lx pts=%lx:%08lx\n",
           activeLoop + 1, (int)(pcl - maPcl), (unsigned long)streamCycle,
           (unsigned long)(scr >> 32), (unsigned long)scr,
           (unsigned long)(pts >> 32), (unsigned long)pts);
}

static void init_pcl(pcl, next, buffer) PCL *pcl;
PCL *next;
char *buffer;
{
    pcl->PCL_Sig = MA_SIG_PCL;
    pcl->PCL_Nxt = next;
    pcl->PCL_Buf = buffer;
    pcl->PCL_BufSz = 1;
    /* ma_cdplay consumes a preloaded sector only when this bit is set. */
    pcl->PCL_Ctrl = PCL_READY;
    pcl->PCL_Err = NULL;
    pcl->PCL_Cnt = 0;
}

static int init_timestamps(source)
LoopSource *source;
{
    int i;
    int firstScr;
    int secondScr;
    int thirdScr;
    unsigned char *sector;
    unsigned long long lastDelta;

    if ((source->length % MPEG_SECTOR_SIZE) != 0 || source->length == 0 ||
        source->length > MA_PCL_MAX * MPEG_SECTOR_SIZE)
        return -1;
    source->pclCount = source->length / MPEG_SECTOR_SIZE;

    /*
     * A PCL is always 2,304 bytes, but MPEG pack/PES boundaries need not
     * align to it.  At lower bitrates a PCL may be a continuation payload;
     * such a sector has no new SCR and/or PTS and is valid.
     */
    firstScr = secondScr = thirdScr = -1;
    for (i = 0; i < source->pclCount; i++) {
        sector = &source->data[i * MPEG_SECTOR_SIZE];
        source->scrOffset[i] = find_pack_scr(sector);
        if (source->scrOffset[i] >= 0) {
            source->scr[i] = get_timestamp(&sector[source->scrOffset[i]]);
            if (firstScr < 0)
                firstScr = i;
            else if (secondScr < 0)
                secondScr = i;
            else if (thirdScr < 0)
                thirdScr = i;
        }
        source->ptsOffset[i] = find_audio_pts(sector);
        if (source->ptsOffset[i] >= 0)
            source->pts[i] = get_timestamp(&sector[source->ptsOffset[i]]);
    }

    if (source->pclCount < 2 || firstScr < 0 || secondScr < 0)
        return -1;

    lastDelta = source->scr[secondScr] - source->scr[firstScr];
    source->firstRegularScr = source->scr[secondScr];
    source->normalScrDelta = lastDelta;
    if (thirdScr >= 0)
        source->normalScrDelta = source->scr[thirdScr] - source->scr[secondScr];
    if (!source->periodTicks)
        source->periodTicks =
            source->scr[secondScr] - source->scr[firstScr] + lastDelta;
    source->hasMuxPreroll =
        firstScr == 0 && source->scr[firstScr] == 0 &&
        source->scr[secondScr] > source->normalScrDelta * 2;
    return source->periodTicks ? 0 : -1;
}

static void retime_sector(index) int index;
{
    LoopSource *source;
    unsigned char *sector;
    unsigned long long scr;
    unsigned long long pts;

    sector = (unsigned char *)&mpegDataBuffer[index * MPEG_SECTOR_SIZE];
    source = &loopSource[activeLoop];
    if (source->hasMuxPreroll && index == 0 && timelineTicks != 0) {
        /* Place a repeated preload pack immediately before pack one. */
        scr = source->firstRegularScr + timelineTicks - source->normalScrDelta;
    } else {
        scr = source->scr[index] + timelineTicks;
    }

    /* Rewrite only timing fields physically present in this PCL. */
    if (source->scrOffset[index] >= 0)
        set_timestamp(&sector[source->scrOffset[index]], scr);
    if (source->ptsOffset[index] >= 0) {
        pts = source->pts[index] + timelineTicks;
        set_timestamp(&sector[source->ptsOffset[index]], pts);
    }
}

static void load_and_retime_sector(index) int index;
{
    LoopSource *source;

    source = &loopSource[activeLoop];
    /* Copy first: retiming must never modify the generated source asset. */
    memcpy(&mpegDataBuffer[index * MPEG_SECTOR_SIZE],
           &source->data[index * MPEG_SECTOR_SIZE], MPEG_SECTOR_SIZE);
    retime_sector(index);
}

static void service_pcls() {
    PCL *pcl;
    char *buffer;

    /* The driver clears/changes a PCL after consuming its preloaded sector. */
    while (maPcl[currentPcl].PCL_Ctrl != PCL_READY) {
        if (currentPcl == 0) {
            /* The completed loop determines both elapsed time and next asset. */
            timelineTicks += loopSource[activeLoop].periodTicks;
            streamCycle++;
            /* Cycles 0..3 use loop 1; 4..7 use loop 2; then repeat. */
            if ((streamCycle % LOOP_REPETITIONS) == 0)
                activeLoop = (activeLoop + 1) % LOOP_COUNT;
        }

        pcl = &maPcl[currentPcl];
        buffer = &mpegDataBuffer[currentPcl * MPEG_SECTOR_SIZE];
        load_and_retime_sector(currentPcl);
        init_pcl(pcl, &maPcl[(currentPcl + 1) % maPclCount], buffer);
        currentPcl = (currentPcl + 1) % maPclCount;
    }
}

void serviceMpeg() {
    if (mpegStatus == MPP_PLAY)
        service_pcls();
}

void initMpeg() {
    char *devName;
    int i;

    /* Associate the generated C arrays with the runtime source descriptors. */
    loopSource[0].data = loop1_mpg;
    loopSource[0].length = loop1_mpg_len;
    loopSource[0].periodTicks = loop1_mpg_period_90k;
    loopSource[1].data = loop2_mpg;
    loopSource[1].length = loop2_mpg_len;
    loopSource[1].periodTicks = loop2_mpg_period_90k;

    /* A fixed PCL ring can switch sources only if their sector counts match. */
    if (init_timestamps(&loopSource[0]) == -1 ||
        init_timestamps(&loopSource[1]) == -1 ||
        loopSource[0].pclCount != loopSource[1].pclCount) {
        printf("Invalid or incompatible embedded MPEG loops\n");
        return;
    }
    maPclCount = loopSource[0].pclCount;

    mpegDataBuffer = (char *)srqcmem(maPclCount * MPEG_SECTOR_SIZE, SYSRAM);
    if (mpegDataBuffer == NULL) {
        printf("Cannot allocate MPEG buffer\n");
        return;
    }

    activeLoop = 0;
    timelineTicks = 0;

    for (i = 0; i < 16; i++) {
        maCil[i] = NULL;
    }

    for (i = 0; i < maPclCount; i++) {
        load_and_retime_sector(i);
        init_pcl(&maPcl[i], &maPcl[(i + 1) % maPclCount],
                 &mpegDataBuffer[i * MPEG_SECTOR_SIZE]);
    }

    currentPcl = 0;
    streamCycle = 0;
    mpegStatus = MPP_STOP;

    devName = csd_devname(DT_MPEGA, 1);
    maPath = open(devName, 0);
    free(devName);
    if (maPath == -1)
        printf("Cannot open MPEG audio device\n");
}

void playMpeg() {
    int channel;

    if (maPath == -1 || maPclCount == 0)
        return;

    channel = MPEG_CHANNEL;
    maMapId = ma_create(maPath, PLAYCD);
    if (maMapId == -1)
        return;

    /* One circular PCL chain is attached to the MPEG audio channel. */
    maCil[channel] = maPcl;
    mpegPcb.PCB_Video = NULL;
    mpegPcb.PCB_Audio = maCil;
    mpegPcb.PCB_Data = NULL;
    mpegPcb.PCB_Sig = MPEG_SIG_PCB;
    mpegPcb.PCB_Chan = 1L << channel;
    mpegPcb.PCB_AChan = 0;
    mpegPcb.PCB_Rec = 0x7fffffff;
    mpegPcb.PCB_Stat = 0;
    maStatus.asy_stat = 0;
    maStatus.asy_sig = MA_SIG_STAT;

    ma_cntrl(maPath, maMapId, 0x00800080, 0L);
    ma_trigger(maPath, MA_SIG_BASE | 0x1f);

    service_pcls();

    dump_pcls("start");

    DEBUG(ma_cdplay(maPath, maMapId, MV_NO_OFFSET, maPcl, &maStatus, MV_NO_SYNC,
                    0));
    mpegStatus = MPP_PLAY;
}

void stopMpeg() {
    if (mpegStatus == MPP_STOP)
        return;
    ma_abort(maPath);
    ma_cntrl(maPath, maMapId, 0x80808080, 0L);
    ma_release(maPath, maMapId);
    mpegStatus = MPP_STOP;
}

int mpegSignal(sigCode)
int sigCode;
{
    if (sigCode == MA_SIG_PCL) {
    } else if ((sigCode & 0xf000) == MA_SIG_BASE) {
        if (sigCode & MA_TRIG_UNF) {
            printf("MPEG audio underflow\n");
            dump_pcls("underflow");
        }
    } else if (sigCode == MA_SIG_STAT) {
        printf("MPEG audio status %x\n", maStatus.asy_stat);
    } else if (sigCode == MPEG_SIG_PCB) {
        printf("MPEG playback ended %x\n", mpegPcb.PCB_Stat);
        stopMpeg();
    }
    return 0;
}
