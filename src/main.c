#include <csd.h>
#include <sysio.h>
#include <signal.h>
#include <ucm.h>
#include <events.h>
#include <stdio.h>
#include <setsys.h>

#include "video.h"
#include "graphics.h"
#include "mpeg.h"

#include "sfx.c"

int mainSignal(sigCode)
int sigCode;
{
	if (sigCode == SIGINT)
	{
		abort();
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
	initGraphics();
	initMpeg();
	initProgram();
}

void closeSystem()
{
	closeVideo();
}

void runProgram()
{
	int evId = _ev_link("line_event");

	setIcf(ICF_MAX, ICF_MAX);
	while (1)
	{
		if (mpegStatus == MPP_STOP)
		{
			printf("Starting FMV\n");
			playMpeg(fma_mpg, sizeof(fma_mpg));
		}

		_ev_wait(evId, 1, 1); /* Wait for VBLANK */
	}
}

int main(argc, argv)
int argc;
char *argv[];
{
	intercept(mainSignal);
	initSystem();
	runProgram();
	closeSystem();
	exit(0);
}
