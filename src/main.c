#include <csd.h>
#include <sysio.h>
#include <ucm.h>
#include <events.h>
#include <stdio.h>
#include <setsys.h>

#include "input.h"
#include "video.h"
#include "graphics.h"
#include "mpeg.h"
#include "cdio.h"

int mainSignal(sigCode)
	int sigCode;
{
	mpegSignal(sigCode);
}

void initProgram() {

}

void initSystem()
{
	initVideo();
	initGraphics();
	initMpeg();
	initInput();
	initProgram();
}

void closeSystem()
{
	closeVideo();
	closeInput();
}

void runProgram() {
	int evId = _ev_link("line_event");

	setIcf(ICF_MAX, ICF_MAX);
	while(1) {
		if (mpegStatus == MPP_STOP) {
			printf("Starting FMV\n");
			playMpeg("/cd/cdipal.rtf", 0);
		}

		_ev_wait(evId, 1, 1); /* Wait for VBLANK */
	}
}

int main(argc, argv)
	int argc;
	char* argv[];
{
	intercept(mainSignal);
	initSystem();
	runProgram();
	closeSystem();
	exit(0);
}
