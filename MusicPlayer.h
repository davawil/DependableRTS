#ifndef PLAYER_H
#define PLAYER_H

#include "TinyTimber.h"
#include "canTinyTimber.h"
#include "sciTinyTimber.h"

#define TONES_IN_TUNE 32
#define TONE_HALF_PERIOD 500
#define TONE_GAP 70
#define KEY_MAX 5
#define KEY_MIN -5
#define TEMPO_MAX 240
#define TEMPO_MIN 60

#define BPM_TO_MSEC(X) 60000/X		//amount of milli seconds per beat provided the bpm
#define MSEC_TO_BPM(X) 60000/X		//beats per minute provided the number of milliseconds

typedef struct{
	Object super;
	int tempo;
	int key;
	int index;
	int enabled;
} MusicPlayer;

int get_tempo(MusicPlayer *, int);
void end_tone(MusicPlayer *, int);
void play_tune(MusicPlayer *self, int unused);
void end_tone(MusicPlayer *self, int unused);
void start_player(MusicPlayer *self, int unused);
void stop_player(MusicPlayer *self, int unused);
int get_player_enabled(MusicPlayer *self, int unused);
void set_player_enabled(MusicPlayer *self, int value);
void set_key(MusicPlayer *self, int value);
void inc_key(MusicPlayer *self, int unused);
void dec_key(MusicPlayer *self, int unused);
void set_tempo(MusicPlayer *self, int value);
void inc_tempo(MusicPlayer *self, int unused);
void dec_tempo(MusicPlayer *self, int unused);
void set_index(MusicPlayer *self, int value);
int get_key(MusicPlayer *self, int unused);
int get_tempo(MusicPlayer *self, int unused);

#endif