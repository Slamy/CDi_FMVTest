#include <strings.h>
#include <csd.h>
#include <sysio.h>
#include <cdfm.h>
#include <stdio.h>
#include <memory.h>
#include <errno.h>
#include "audio.h"
#include "sine_adpcm.h"

#define DEBUG(c)                         \
    if ((c) == -1)                       \
    {                                    \
        printf("FAIL: c (%d)\n", errno); \
    }

int audioPath;
int smId;
STAT_BLK smStat;

void initAudio()
{
    char *Buffer;
    char *devName = csd_devname(DT_AUDIO, 1); /* Get Audio Device Name */
    audioPath = open(devName, UPDAT_);        /* Open Audio Device */
    free(devName);                            /* Release memory */
    /* NOTE SC_ATTEN( R->L, R->R, L->R, L->L) */
    DEBUG(sc_atten(audioPath, 0x00800080)); /* Full Mix */
    printf("size of sample is %d %x\n", sizeof(sine_adpcm), &sine_adpcm);

    smId = sm_creat(audioPath, D_CMONO, sizeof(sine_adpcm) * 18 / 2304, &Buffer);
    memcpy(Buffer, sine_adpcm, sizeof(sine_adpcm));

    DEBUG(smId);
    smStat.asy_sig = 0;  /*SIG_AUDIO_SM;*/
    smStat.asy_stat = 0; /*SIG_AUDIO_SM;*/
}

void startAudio(unsigned long attenuation)
{
    /* DEBUG(sd_loop(audioPath, smId, 0, 18 * 2 - 1, 0x0FFF)); */
    /* printf("Play audiomap\n"); */
    DEBUG(sc_atten(audioPath, attenuation)); /* Full Mix */
    DEBUG(sm_out(audioPath, smId, &smStat));
}
