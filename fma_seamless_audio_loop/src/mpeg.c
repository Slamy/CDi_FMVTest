#include <cdfm.h>
#include <csd.h>
#include <ma.h>
#include <memory.h>
#include <mv.h>
#include <stdio.h>
#include <sysio.h>
#include <ucm.h>

#include "graphics.h"
#include "hwreg.h"
#include "mpeg.h"
#include "sfx1.c"
#include "sfx2.c"
#include "sfx_restart.c"
#include "sfx_start.c"
#include "song_sequence.inc"

/*
 * This is an audio-only MPEG Program Stream player for CD-i.
 *
 * build.sh produces independent MP2/Program-Stream parts and a generated
 * songSequence table.  Every entry in that table points at exactly one MPEG
 * sector, so parts may be repeated or entered at an arbitrary sector.
 * PCL buffers are reused only after the MPEG driver has consumed them.
 */

extern int errno;

#define CHECK(c, label)                                                                            \
    if ((c) == -1) {                                                                               \
        printf("MPEG error: %s (errno %d)\n", label, errno);                                       \
    }

#define PCL_READY 0x01

int mpegStatus;
int maPath, maMapId;
int mvPath, mvMapId; /* Kept for the application's public interface. */

static PCB mpegPcb;
static PCL maPcl[MA_PCL_MAX];
static PCL *maCil[16];
static STAT_BLK maStatus;

static int maPclCount;
static int currentPcl; /* next returned PCL to refill */
static int nextSequenceSector;

#define SOURCE_COUNT 4
#define MAX_SOURCE_SECTORS 16

typedef struct {
    char *name;
    unsigned char *data;
    unsigned long length;
    unsigned long periodTicks;
    int sectorCount;
    unsigned long long scr[MAX_SOURCE_SECTORS];
    unsigned long long pts[MAX_SOURCE_SECTORS];
    int scrOffset[MAX_SOURCE_SECTORS];
    int ptsOffset[MAX_SOURCE_SECTORS];
    unsigned long long firstRegularScr;
    unsigned long long normalScrDelta;
    int hasMuxPreroll;
} SongSource;

static SongSource source[SOURCE_COUNT];
static SongSource *partSource;
static unsigned long long timelineTicks;
static unsigned long long partTrimTicks;
static unsigned long long lastScr;
static int partStartSector;
static int sequenceStarted;
static int haveLastScr;

static unsigned long long get_timestamp(p)
unsigned char *p;
{
    return ((unsigned long long)((p[0] >> 1) & 7) << 30) | ((unsigned long long)p[1] << 22) |
           ((unsigned long long)(p[2] & 0xfe) << 14) | ((unsigned long long)p[3] << 7) |
           ((unsigned long long)(p[4] & 0xfe) >> 1);
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
    static int offsets[] = {0, 12, 27};
    int i;
    int offset;

    /* MPEG Program Stream packet headers from embed_mpeg.py occur only at
     * these positions.  Searching arbitrary MP2 payload bytes can mistake a
     * compressed-audio bit pattern for a PES header and corrupt it. */
    for (i = 0; i < 3; i++) {
        offset = offsets[i];
        if (sector[offset] == 0 && sector[offset + 1] == 0 && sector[offset + 2] == 1 &&
            sector[offset + 3] == 0xc0 && (sector[offset + 6] & 0xf0) == 0x20)
            return offset + 6;
    }
    return -1;
}

static int find_pack_scr(sector)
unsigned char *sector;
{
    int offset;
    for (offset = 0; offset <= MPEG_SECTOR_SIZE - 9; offset++)
        if (sector[offset] == 0 && sector[offset + 1] == 0 && sector[offset + 2] == 1 &&
            sector[offset + 3] == 0xba)
            return offset + 4;
    return -1;
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

static int init_timestamps(s)
SongSource *s;
{
    int i;
    int first;
    int second;
    int third;
    int firstPts;
    unsigned char *sector;

    if (!s->length) {
        printf("MPEG error: %s is empty\n", s->name);
        return -1;
    }
    if (s->length % MPEG_SECTOR_SIZE) {
        printf("MPEG error: %s length %lu is not sector aligned\n", s->name, s->length);
        return -1;
    }
    if (s->length / MPEG_SECTOR_SIZE > MAX_SOURCE_SECTORS) {
        printf("MPEG error: %s has too many sectors (%lu, max %d)\n", s->name,
               s->length / MPEG_SECTOR_SIZE, MAX_SOURCE_SECTORS);
        return -1;
    }
    s->sectorCount = s->length / MPEG_SECTOR_SIZE;
    first = second = third = firstPts = -1;
    for (i = 0; i < s->sectorCount; i++) {
        sector = s->data + i * MPEG_SECTOR_SIZE;
        s->scrOffset[i] = find_pack_scr(sector);
        s->ptsOffset[i] = find_audio_pts(sector);
        if (s->scrOffset[i] >= 0) {
            s->scr[i] = get_timestamp(sector + s->scrOffset[i]);
            if (first < 0)
                first = i;
            else if (second < 0)
                second = i;
            else if (third < 0)
                third = i;
        }
        if (s->ptsOffset[i] >= 0) {
            s->pts[i] = get_timestamp(sector + s->ptsOffset[i]);
            if (firstPts < 0)
                firstPts = i;
        }
    }
    if (first < 0) {
        printf("MPEG error: %s contains no pack SCR\n", s->name);
        return -1;
    }
    if (firstPts < 0) {
        printf("MPEG error: %s contains no audio PTS\n", s->name);
        return -1;
    }
    if (second < 0) {
        /* The three-frame intro is a valid one-sector Program Stream. */
        s->firstRegularScr = s->scr[first];
        s->normalScrDelta = 0;
        s->hasMuxPreroll = 0;
        return 0;
    }
    s->firstRegularScr = s->scr[second];
    s->normalScrDelta = s->scr[second] - s->scr[first];
    if (third >= 0)
        s->normalScrDelta = s->scr[third] - s->scr[second];
    s->hasMuxPreroll = first == 0 && s->scr[first] == 0 && s->scr[second] > s->normalScrDelta * 2;
    return 0;
}

static SongSource *source_for_sector(sector, sectorIndex)
unsigned char *sector;
int *sectorIndex;
{
    int i;
    for (i = 0; i < SOURCE_COUNT; i++)
        if (sector >= source[i].data && sector < source[i].data + source[i].length) {
            *sectorIndex = (sector - source[i].data) / MPEG_SECTOR_SIZE;
            return &source[i];
        }
    return NULL;
}

static unsigned long long first_pts(s, from)
SongSource *s;
int from;
{
    int i;
    for (i = from; i < s->sectorCount; i++)
        if (s->ptsOffset[i] >= 0)
            return s->pts[i];
    return 0;
}

static void start_part(s, sectorIndex) SongSource *s;
int sectorIndex;
{
    if (sequenceStarted)
        timelineTicks += partSource->periodTicks - partTrimTicks;
    sequenceStarted = 1;
    partSource = s;
    partStartSector = sectorIndex;
    partTrimTicks = first_pts(s, sectorIndex) - first_pts(s, 0);
}

static void load_sector(index) int index;
{
    unsigned char *sector;
    SongSource *currentSource;
    int sectorIndex;
    int previous;
    int newPart;
    unsigned long long scr;
    unsigned long long pts;

    sector = songSequence[nextSequenceSector];
    scr = pts = 0;
    currentSource = source_for_sector(sector, &sectorIndex);
    if (currentSource == NULL) {
        printf("MPEG error: sequence %d points outside every source\n", nextSequenceSector);
        return;
    }
    previous = (nextSequenceSector + SONG_SEQUENCE_SECTOR_COUNT - 1) % SONG_SEQUENCE_SECTOR_COUNT;
    newPart = !sequenceStarted || currentSource != partSource ||
              sector != songSequence[previous] + MPEG_SECTOR_SIZE;
    if (newPart) {
        start_part(currentSource, sectorIndex);
    }

    /* With five PCLs, this sector cannot still be owned by the decoder when
     * it appears again in this song.  Retiming it in-place removes memcpy. */
    if (currentSource->scrOffset[sectorIndex] >= 0) {
        if (currentSource->hasMuxPreroll && sectorIndex == 0 && partStartSector == 0 &&
            timelineTicks != 0)
            scr = currentSource->firstRegularScr + timelineTicks - currentSource->normalScrDelta;
        else
            scr = currentSource->scr[sectorIndex] - partTrimTicks + timelineTicks;
        if (haveLastScr && scr <= lastScr)
            scr = lastScr + 1;
        set_timestamp(sector + currentSource->scrOffset[sectorIndex], scr);
        lastScr = scr;
        haveLastScr = 1;
    }
    if (currentSource->ptsOffset[sectorIndex] >= 0) {
        pts = currentSource->pts[sectorIndex] - partTrimTicks + timelineTicks;
        set_timestamp(sector + currentSource->ptsOffset[sectorIndex], pts);
    }
    maPcl[index].PCL_Buf = (char *)sector;
    nextSequenceSector++;
    if (nextSequenceSector == SONG_SEQUENCE_SECTOR_COUNT) {
        /* start.mp2 is a one-time intro; subsequent passes begin at restart. */
        nextSequenceSector = SONG_SEQUENCE_REPEAT_START;
    }
}

static void service_pcls() {
    PCL *pcl;

    /* The driver clears/changes a PCL after consuming its preloaded sector. */
    while (maPcl[currentPcl].PCL_Ctrl != PCL_READY) {
        pcl = &maPcl[currentPcl];
        if (pcl->PCL_Err != NULL)
            printf("MPEG error: PCL %d reported an error\n", currentPcl);
        load_sector(currentPcl);
        init_pcl(pcl, &maPcl[(currentPcl + 1) % maPclCount], maPcl[currentPcl].PCL_Buf);
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

    /* Leave later play attempts in a diagnosable stopped state on any error. */
    maPath = -1;
    maPclCount = 0;

    source[0].data = start_mpg;
    source[0].name = "start";
    source[0].length = start_mpg_len;
    source[0].periodTicks = start_mpg_period_90k;
    source[1].data = loop1_mpg;
    source[1].name = "loop1";
    source[1].length = loop1_mpg_len;
    source[1].periodTicks = loop1_mpg_period_90k;
    source[2].data = loop2_mpg;
    source[2].name = "loop2";
    source[2].length = loop2_mpg_len;
    source[2].periodTicks = loop2_mpg_period_90k;
    source[3].data = restart_mpg;
    source[3].name = "restart";
    source[3].length = restart_mpg_len;
    source[3].periodTicks = restart_mpg_period_90k;
    for (i = 0; i < SOURCE_COUNT; i++) {
        if (init_timestamps(&source[i]) == -1) {
            printf("MPEG error: cannot initialize source %s\n", source[i].name);
            return;
        }
    }
    if (SONG_SEQUENCE_SECTOR_COUNT == 0 || SONG_SEQUENCE_REPEAT_START < 0 ||
        SONG_SEQUENCE_REPEAT_START >= SONG_SEQUENCE_SECTOR_COUNT) {
        printf("MPEG error: invalid song sequence (count %d repeat %d)\n",
               SONG_SEQUENCE_SECTOR_COUNT, SONG_SEQUENCE_REPEAT_START);
        return;
    }
    maPclCount = SONG_SEQUENCE_SECTOR_COUNT;
    if (maPclCount > MA_PCL_MAX)
        maPclCount = MA_PCL_MAX;

    nextSequenceSector = 0;
    timelineTicks = 0;
    haveLastScr = 0;
    sequenceStarted = 0;

    for (i = 0; i < 16; i++) {
        maCil[i] = NULL;
    }

    for (i = 0; i < maPclCount; i++) {
        load_sector(i);
        init_pcl(&maPcl[i], &maPcl[(i + 1) % maPclCount], maPcl[i].PCL_Buf);
    }

    currentPcl = 0;
    mpegStatus = MPP_STOP;

    devName = csd_devname(DT_MPEGA, 1);
    if (devName == NULL) {
        printf("MPEG error: no MPEG audio device name\n");
        return;
    }
    maPath = open(devName, 0);
    free(devName);
    if (maPath == -1)
        printf("MPEG error: cannot open MPEG audio device (errno %d)\n", errno);
}

void playMpeg() {
    int channel;

    if (maPath == -1 || maPclCount == 0) {
        printf("MPEG error: cannot play (path %d, PCL count %d)\n", maPath, maPclCount);
        return;
    }

    channel = MPEG_CHANNEL;
    maMapId = ma_create(maPath, PLAYCD);
    if (maMapId == -1) {
        printf("MPEG error: ma_create failed (errno %d)\n", errno);
        return;
    }

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

    CHECK(ma_cntrl(maPath, maMapId, 0x00800080, 0L), "ma_cntrl(play)");
    CHECK(ma_trigger(maPath, MA_SIG_BASE | 0x1f), "ma_trigger");

    service_pcls();

    /* Finish the complete first graphics frame before MPEG playback begins. */
    graphicsStartSong();
    CHECK(ma_cdplay(maPath, maMapId, MV_NO_OFFSET, maPcl, &maStatus, MV_NO_SYNC, 0), "ma_cdplay");
    mpegStatus = MPP_PLAY;
}

void stopMpeg() {
    if (mpegStatus == MPP_STOP)
        return;
    CHECK(ma_abort(maPath), "ma_abort");
    CHECK(ma_cntrl(maPath, maMapId, 0x80808080, 0L), "ma_cntrl(stop)");
    CHECK(ma_release(maPath, maMapId), "ma_release");
    mpegStatus = MPP_STOP;
}

int mpegSignal(sigCode)
int sigCode;
{
    if (sigCode == MA_SIG_PCL) {
    } else if ((sigCode & 0xf000) == MA_SIG_BASE) {
        if (sigCode & MA_TRIG_UNF) {
            printf("MPEG audio underflow\n");
        }

        if (sigCode & MA_TRIG_DEC) {
            /* printf("D\n"); */
        }
        if (sigCode & MA_TRIG_UPD) {
            /* One update arrives for every decoded MPEG audio frame. */
            graphicsAudioUpdate();
        }
    } else if (sigCode == MA_SIG_STAT) {
        printf("MPEG audio status %x\n", maStatus.asy_stat);
    } else if (sigCode == MPEG_SIG_PCB) {
        printf("MPEG playback ended %x\n", mpegPcb.PCB_Stat);
        stopMpeg();
    }
    return 0;
}
