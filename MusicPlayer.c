#include "MusicPlayer.h"
#include "ToneGen.h"

#define BROTHER_JOHN_TUNE {0,2,4,0,0,2,4,0,4,5,7,4,5,7,7,9,7,5,4,0,7,9,7,5,4,0,0,-5,0,0,-5,0}
#define BROTHER_JOHN_HALF_BEATS {2,2,2,2,2,2,2,2,2,2,4,2,2,4,1,1,1,1,2,2,1,1,1,1,2,2,2,2,4,2,2,4}

int tone_ind[TONES_IN_TUNE] = BROTHER_JOHN_TUNE;
const int tone_periods[25] = {2024, 1911, 1803, 1702, 1607, 1516, 1431, 1351, 1275, 1203, 1136, 1072, 1012, 955, 901, 851, 803, 758, 715, 675, 637, 601, 568, 536, 506};
const int tone_beats[TONES_IN_TUNE] = BROTHER_JOHN_HALF_BEATS;

extern ToneGen toneGen;

void play_tune(MusicPlayer *self, int unused){
	if(self->enabled){
		int period = tone_periods[tone_ind[self->index] + 10 + self->key];
		self->tone_period = period;
		SYNC(&toneGen, setAlive, 1);
		ASYNC(&toneGen, wave, period);
		int tempo = self->tempo;
		if(self->modulated == MOD_TEMPO)
			tempo = self->mod_tempo;
		int delay = (BPM_TO_MSEC(tempo) * tone_beats[self->index])/2;
		SEND(MSEC(delay-50),MSEC(50),self, end_tone, 0);		
	}
}

void end_tone(MusicPlayer *self, int unused){
	//disable tone
	SYNC(&toneGen, setAlive, 0);
	//delay before next tone
	self->index++;
	if(self->index >= TONES_IN_TUNE)
		self->index = 0;
		
	SEND(MSEC(50), MSEC(50),self, play_tune, 0);
}
void start_player(MusicPlayer *self, int unused){
	self->enabled = 1;
	self->index = 0;
	//ASYNC(&player, set_LED, 0);
	ASYNC(self, play_tune, unused);
}
void stop_player(MusicPlayer *self, int unused){
	self->enabled = 0;
}
int get_player_enabled(MusicPlayer *self, int unused){
	return self->enabled;
}
void set_player_enabled(MusicPlayer *self, int value){
	self->enabled = value;
}
void set_key(MusicPlayer *self, int value){
	self->key = value;
}
void inc_key(MusicPlayer *self, int unused){
	if(self->key < KEY_MAX)
		self->key++;
}
void dec_key(MusicPlayer *self, int unused){
	if(self->key > KEY_MIN)
		self->key--;
}
void set_tempo(MusicPlayer *self, int value){
	self->tempo = value;
}
void inc_tempo(MusicPlayer *self, int unused){
	if(self->tempo < TEMPO_MAX)
		self->tempo+=10;
}
void dec_tempo(MusicPlayer *self, int unused){
	if(self->tempo > TEMPO_MIN)
		self->tempo-=10;
}
void set_index(MusicPlayer *self, int value){
	self->index = value;
}
int get_key(MusicPlayer *self, int unused){
	return self->key;
}
int get_tempo(MusicPlayer *self, int unused){
	return self->tempo;
}
void set_mod_tempo(MusicPlayer *self, int value){
	if(value > TEMPO_MAX)
		self->mod_tempo = TEMPO_MAX;
	else
		self->mod_tempo = value;
}

void set_modulated(MusicPlayer *self, int value){
	self->modulated = value;
}
int get_tone_period(MusicPlayer *self, int value){
	return self->tone_period;
}