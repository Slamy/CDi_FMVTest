#include <csd.h>
#include <sysio.h>
#include <ucm.h>
#include <events.h>
#include <stdio.h>
#include <setsys.h>

#include "video.h"
#include "audio.h"
#include "graphics.h"
#include "mpeg.h"
#include <signal.h>

int exit_app = 0;

int mainSignal(sigCode)
int sigCode;
{
	if (sigCode == SIGINT)
	{
		printf("SIGINT!\n");
		exit_app = 1;
	}
	else
	{
		mpegSignal(sigCode);
	}
}

void initProgram()
{
}

void initSystem()
{
	initVideo();
	initAudio();
	initGraphics();
	initMpeg();
	initProgram();
}

void closeSystem()
{
	closeVideo();
}

void testVolume(unsigned long attenuation)
{
	if (!exit_app)
	{
		sleep(1);
		StartPlayback(attenuation);
		startAudio(attenuation);
		while (playback_has_ended == 0)
			;
	}
}
void runProgram()
{
	unsigned long atten;
	unsigned long i;

	dc_ssig(videoPath, SIG_BLANK, 0);
	playMpeg(0x00800080);

	testVolume(0x00800080); /* Normal L2L and R2R */
	testVolume(0x80008000); /* Swap left and right */
	testVolume(0x80800080); /* Only right */
	testVolume(0x00808080); /* Only left */

	testVolume(0x00000000); /* All On - Evil clipping Test */
	testVolume(0x05050505); /* All On - Evil clipping Test */
#if 0
	StartPlayback(0x10101010); /* All On - Evil clipping Test */
	while (playback_has_ended == 0)
		;
#endif

	for (i = 0; i < 30; i += 1)
	{
		atten = (i << 24) | (i << 8) | 0x00800080;
		testVolume(atten);
	}
	printf("Finished!\n");
	while (!exit_app)
	{
	}
}

extern int os9forkc();
extern char **environ;
char *argblk[] = {
	"vcd",
	0,
};

int main(argc, argv)
int argc;
char *argv[];
{
	intercept(mainSignal);

	initSystem();
	runProgram();
	closeSystem();

	sleep(1);
	exit(0);
}
