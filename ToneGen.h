#ifndef TONE_GEN_H
#define TONE_GEN_H

#define MAX_VOLUME 20
#define DIG_AN_CONV ((volatile unsigned int*) 0x4000741C)
#include "TinyTimber.h"
#include "sciTinyTimber.h"

typedef struct{
	Object super;
	unsigned char volume;
	Time deadline;
	int alive;
	int wave;
}ToneGen;

void wave(ToneGen* self, int unused);
void raiseVol(ToneGen *self, int unused);
void lowerVol(ToneGen *self, int unused);
void setAlive(ToneGen *self, int unused);

#endif