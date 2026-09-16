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

#include <signal.h>

int exit_app=0;

int mainSignal(sigCode)
int sigCode;
{
	if (sigCode == SIGINT)
	{
		printf("SIGINT!\n");
		exit_app=1;
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

	while (!exit_app)
	{
		/* The MPEG driver does not reliably signal every completed PCL on all
		 * players, so also recycle the in-memory Program Stream once per VBL. */
		serviceMpeg();
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
