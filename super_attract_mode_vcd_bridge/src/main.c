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
#include "hwreg.h"

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

void initSystem()
{
	initVideo();
	initMpeg();
}

void closeSystem()
{
	closeVideo();
}

void runProgram()
{
	int evId = _ev_link("line_event");

	setIcf(ICF_MAX, ICF_MAX);
	while (!exit_app)
	{
		if (mpegStatus == MPP_STOP)
		{
			printf("Starting FMV\n");
			playMpeg();
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

	sleep(1);
	exit(0);
}
