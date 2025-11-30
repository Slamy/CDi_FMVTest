#ifndef __MPEG_H__
#define __MPEG_H__

#define MV_PCL_COUNT 75
#define MA_PCL_COUNT 5
#define MPEG_SECTOR_SIZE 2324
#define MPEG_CHANNEL 0

#define MV_SPEED_SINGLE_STEP 0x7FFFFFFF
#define MV_SPEED_SCAN 0x80000000
#define MV_SPEED_SLOW_MOTION 0x00000002
#define MV_SPEED_NORMAL 0x00000000

#define MA_AUD_MODE 0x000000C0
#define MA_AUD_STEREO 0x00000000
#define MA_AUD_JOINT 0x00000040
#define MA_AUD_DUAL 0x00000080
#define MA_AUD_SINGLE 0x000000C0

/* literals for the video board */
#define MV_NO_OFFSET 0x00
#define MV_NO_SYNC -1

#define MA_SIG_BASE 0xC000 /* Signal base */

/* literals for the different signal mechanism */
#define MV_SIG_BASE 0xB000 /* Signal base */
#define MV_TRIG_DER 0x0001 /* Data Error */
#define MV_TRIG_PIC 0x0002 /* Picture Displayed */
#define MV_TRIG_GOP 0x0004 /* Group of Pictures */
#define MV_TRIG_SOS 0x0008 /* Start of Seq */
#define MV_TRIG_LPD 0x0010 /* Last Picture Displayed */
#define MV_TRIG_CNP 0x0020 /* Old PCL not used */
#define MV_TRIG_EOI 0x0040 /* End of ISO strm */
#define MV_TRIG_EOS 0x0080 /* End of Sequence */
#define MV_TRIG_BUF 0x0100 /* Buffer underflow */
#define MV_TRIG_NIS 0x0200 /* New Sequence Parms */

/*#define MV_TRIG_MASK	(MV_SIG_BASE+MV_TRIG_PIC+MV_TRIG_BUF+MV_TRIG_NIS)*/
#define MV_TRIG_MASK (MV_SIG_BASE + 0x03FF)

#define MA_SIG_PCL 0x1A00
#define MA_SIG_STAT 0x1A01
#define MV_SIG_PCL 0x1B00
#define MV_SIG_STAT 0x1B01
#define MPEG_SIG_PCB 0x1C00

#define MPP_STOP 0
#define MPP_INIT 1
#define MPP_PLAY 2

extern int mpegStatus;
extern int mvPath, maPath;
extern int mvMapId, maMapId;

#endif