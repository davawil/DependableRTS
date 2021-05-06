#include "ToneGen.h"
Time hp = USEC(1072);
extern Serial sci0;
void wave(ToneGen *self, int period){
	if(self->alive){
		int realPeriod = period;
		if(self->modulated == MOD_PERIOD)
			realPeriod = self->modPeriod;			
		if(self->muted == 0) {
			if(self->wave){
				if(self->modulated == MOD_VOL)
					*DIG_AN_CONV = self->modVolume;
				else 
					*DIG_AN_CONV = self->volume;					
				self->wave = 0;
			}
			else{
				*DIG_AN_CONV = 0;
				self->wave = 1;
			}
		}
		SEND(USEC(realPeriod), self->deadline, self, wave, period);
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

void setModVol(ToneGen *self, int value){
	if(value <= MAX_VOLUME && value >= 0)
		self->modVolume = value;
	else if(value > MAX_VOLUME)
		self->modVolume = MAX_VOLUME;
}
void setModulated(ToneGen *self, int value){
	self->modulated = value;
}
int getVol(ToneGen *self, int unused){
	return self->volume;
}
void setModPeriod(ToneGen *self, int value){
	self->modPeriod = value;
}