#include <csd.h>
#include <sysio.h>
#include <ucm.h>
#include <events.h>
#include <stdio.h>
#include <setsys.h>

#include "video.h"
#include "input.h"
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
	initInput();
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
	u_short input, last_input;
	dc_ssig(videoPath, SIG_BLANK, 0);

	while (!exit_app)
	{
		if (mpegStatus == MPP_STOP)
		{
			printf("Starting FMV\n");
			playMpeg("/cd/VIDEO01.RTF", 0);
		}

		input = readInput1();

		if ((last_input & I_BUTTON1) == 0 && (input & I_BUTTON1))
		{
			raster_equal_to_fmv = !raster_equal_to_fmv;
			printf("Raster == FMV %d\n", raster_equal_to_fmv);
		}

		last_input = input;
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

	sleep(1);
	exit(0);
}
