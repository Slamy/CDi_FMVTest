#include <csd.h>
#include <sysio.h>
#include <ucm.h>
#include <events.h>
#include <stdio.h>
#include <setsys.h>

#include "video.h"
#include "graphics.h"
#include "mpeg.h"
#include "input.h"
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
	else if (sigCode == I_SIGNAL1)
	{
		int curState = readInput1();
		input1State = (input1State & 0xF0) | curState;
		/* printf("I_SIGNAL1 %x!\n", input1State); */

		pt_ssig(input1Path, sigCode);
	}
	else if (sigCode == I_SIGNAL2)
	{
		int curState = readInput2();
		/* printf("I_SIGNAL2!\n"); */

		input2State = (input2State & 0xF0) | curState;
		pt_ssig(input2Path, sigCode);
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
	initInput();
	setInputSignals();
}

void closeSystem()
{
	closeVideo();
}

void runProgram()
{
	dc_ssig(videoPath, SIG_BLANK, 0);

	while (!exit_app)
	{
		if (mpegStatus == MPP_STOP)
		{
			printf("Starting FMV\n");
			playMpeg("/cd/VIDEO01.RTF", 0);
		}
		readInput();
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
