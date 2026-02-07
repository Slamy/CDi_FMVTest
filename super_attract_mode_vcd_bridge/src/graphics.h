#ifndef __GRAPHICS_H__
#define __GRAPHICS_H__

#include "video.h"

extern int curIcfA, curIcfB;

extern u_char *paVideo1;
extern u_char *paVideo2;

void setIcf(icfA, icfB);

#endif