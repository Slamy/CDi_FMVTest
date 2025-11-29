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

int mainSignal(sigCode)
int sigCode;
{
	if (sigCode == SIGINT)
	{
		sleep(1);
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

	while (1)
	{
		if (mpegStatus == MPP_STOP)
		{
			printf("Starting FMV\n");
			playMpeg("/cd/VIDEO01.RTF", 0);
		}

		_ev_wait(evId, 1, 1); /* Wait for VBLANK */
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
	/* system("vcd"); */
	int pid;
	/*
	 * My VMPEG DVC starts in VCD mode.
	 * When stub loading the application, it is not configured to Green Book resolution.
	 * Manually starting the vcd application fixes this problem.
	 */
	if ((pid = os9exec(os9forkc, argblk[0], argblk, environ, 0, 0, 3)) > 0)
		wait(0);
	else
		printf("cant fork\n");

	intercept(mainSignal);
	initSystem();
	runProgram();
	closeSystem();
	exit(0);
}
