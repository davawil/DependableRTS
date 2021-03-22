#include "MusicPlayer.h"
#include <stdio.h>
#include "ToneGen.h"
#define BEAT 500
#define H_BEAT 250
#define D_BEAT 1000


extern Serial sci0;
extern int indicies[32];
extern int periods[25];
int beats[32] = {BEAT,BEAT,BEAT,BEAT,BEAT,BEAT,BEAT,BEAT,BEAT,BEAT,D_BEAT,BEAT,BEAT,D_BEAT,H_BEAT,H_BEAT,H_BEAT,H_BEAT,BEAT,BEAT,H_BEAT,H_BEAT,H_BEAT,H_BEAT,BEAT,BEAT,BEAT,BEAT,D_BEAT,BEAT,BEAT,D_BEAT};

extern ToneGen tonGen;
extern Can can0;
void playTone(MusicPlayer *self, int unused){
	if(self->alive){
		if(self->mode == LEADER_MODE){
			self->beat += beats[self->NextTone];
			self->beat = self->beat%2000;
			if(self->NextTone == 4){
				CANMsg msg;				
				msg.nodeId = LEADER_MODE;
				msg.length = 2;
				msg.msgId = 0;
				msg.buff[0] = SLAVE_MODE;
				CAN_SEND(&can0, &msg);
			}
			if(self->NextTone == 8){
				CANMsg msg;				
				msg.nodeId = LEADER_MODE;
				msg.length = 2;
				msg.msgId = 0;
				msg.buff[0] = SECOND_SLAVE_MODE;
				CAN_SEND(&can0, &msg);
			}
		}
		
		int fIndex = indicies[self->NextTone];
		int period = periods[10 + fIndex + self->key];
		SYNC(&tonGen, setAlive, 1);
		BEFORE(USEC(100), &tonGen, wave, period);
		float factor = 120/(float)self->tempo;
		Time delay = MSEC((int)(factor*beats[self->NextTone]))-self->gap;
		SEND(delay,self->ToneDeadline, self, endTone, 0);
	}	
}

void endTone(MusicPlayer *self, int unused){
	
	BEFORE(self->ToneDeadline,&tonGen, setAlive, 0);
	if(self->NextTone == 31){
		self->NextTone = 0;
	}else{
		self->NextTone++;
		if(self->mode == LEADER_MODE){
			if(self->beat == 0){
				self->tempo = self->tempoBuf;
				CANMsg msg;
				msg.nodeId = LEADER_MODE;
				msg.length = 2;
				msg.msgId = 3;
				msg.buff[0] = (unsigned char) self->tempo;
				msg.buff[1] = 0;
				CAN_SEND(&can0, &msg);
			}
		}
	}
	SEND(self->gap,self->ToneDeadline, self, playTone, 0);
}

void setTempoBuf(MusicPlayer *self, int tempo){
	if(tempo >= 60 && tempo <= 240){
		self->tempoBuf = tempo;
	}	
}
void setTempo(MusicPlayer *self, int tempo){
	self->tempo = tempo;
}
void setKey(MusicPlayer *self, int key){
	if(key >= -5 && key<=5){
		self->key = key;
	}
}
void toggleMusic(MusicPlayer *self, int unused){
	if(self->alive){
		stopMusic(self,unused);
	}
	else{
		startMusic(self,unused);
	}
}
void changeStartPos(MusicPlayer *self, int n){
	if(n>0 && n<32){
		self->startPos = 0;
	}
	
}
void startMusic(MusicPlayer *self, int unused){
	self->alive = 1;
	self->NextTone = 0;
	BEFORE(self->ToneDeadline, self, playTone,0);
}
void stopMusic(MusicPlayer *self, int unused){
	self->alive = 0;
}
