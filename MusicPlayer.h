#ifndef PLAYER_H
#define PLAYER_H

#define SLAVE_MODE 1
#define LEADER_MODE 0
#define SECOND_SLAVE_MODE 2

#include "TinyTimber.h"
#include "canTinyTimber.h"
#include "sciTinyTimber.h"
typedef struct{
	Object super;
	int mode;
	int tempo;
	Time gap;
	int key;
	int NextTone;			//next index in tone-array
	Time ToneDeadline;
	int alive;
	int startPos;
	int tempoBuf;
	int beat;
}MusicPlayer;

void playTone(MusicPlayer *self, int unused);
void endTone(MusicPlayer *self, int unused);
void setTempo(MusicPlayer *self, int tempo);
void setKey(MusicPlayer *self, int key);
void toggleMusic(MusicPlayer *self, int unused);
void changeStartPos(MusicPlayer *self, int n);
void startMusic(MusicPlayer *self, int unused);
void stopMusic(MusicPlayer *self, int unused);
void setTempoBuf(MusicPlayer *self, int tempo);

#endif