#ifndef __GRAPHICS_H__
#define __GRAPHICS_H__

#include "video.h"

extern int curIcfA, curIcfB;
extern unsigned char *paVideo1, *paVideo2;

void setIcf(icfA, icfB);
void buildImage(source, target);

#endif