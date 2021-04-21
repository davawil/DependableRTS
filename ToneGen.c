#include "ToneGen.h"
Time hp = USEC(1072);
extern Serial sci0;
void wave(ToneGen *self, int halfperiod){
	if(self->alive){
		if(self->muted == 0) {
				if(self->wave){
				*DIG_AN_CONV = self->volume;
				self->wave = 0;
			}
			else{
				*DIG_AN_CONV = 0;
				self->wave = 1;
			}
		}
		SEND(USEC(halfperiod), self->deadline, self, wave, halfperiod);
	}
	
}

void raiseVol(ToneGen *self, int unused){
	if(self->volume < MAX_VOLUME){
		self->volume++;
	}
}
void lowerVol(ToneGen *self, int unused){
	if(self->volume > 0){
		self->volume--;
	}
}
 
void setAlive(ToneGen *self, int alive){
	self->alive = alive;
}

int getMuted(ToneGen *self,int unused) {
	return self->muted;
}

void setMuted(ToneGen *self,int value) {
	self->muted = value;
}