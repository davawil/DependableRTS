#include "TinyTimber.h"
#include "sciTinyTimber.h"
#include "canTinyTimber.h"
#include <stdlib.h>
#include <stdio.h>
#include "MusicPlayer.h"
#include "ToneGen.h"

#define MAX_INDEX 14
#define MIN_INDEX -10

#define TONE_DEADLINE USEC (100)
#define BIG_DEADLINE MSEC(20)


int indicies[32] = {0,2,4,0,0,2,4,0,4,5,7,4,5,7,7,9,7,5,4,0,7,9,7,5,4,0,0,-5,0,0,-5,0};
int periods[25] = {2025, 1911, 1804, 1703, 1607, 1517, 1432, 1351, 1276, 1204, 1136,1072, 1012,	955, 901, 851, 803,	758, 715, 675, 637,	601, 568, 536, 506};

typedef struct {
    Object super;
    int count;
    char c;
	char buf[20];	
} App;

MusicPlayer player = {initObject(),LEADER_MODE,120,MSEC(50),0,0,MSEC(1),0,0,120};
ToneGen tonGen = {initObject(),5,TONE_DEADLINE,0,0};

App app = { initObject(), 0, 'X'};

void reader(App*, int);
void receiver(App*, int);

Serial sci0 = initSerial(SCI_PORT0, &app, reader);

Can can0 = initCan(CAN_PORT0, &app, receiver);


void receiver(App *self, int unused) {
    CANMsg msg;
    CAN_RECEIVE(&can0, &msg);
    SCI_WRITE(&sci0, "Can msg received: ");
	char id[2];
	sprintf(id,"%d",(int)msg.msgId);
    SCI_WRITE(&sci0, id);
	char data[4];
	sprintf(data,"%d",(int)msg.buff[0]);
	SCI_WRITE(&sci0, data);
	SCI_WRITE(&sci0, "\n");
	if(player.mode == SLAVE_MODE){
		if(msg.msgId == 0){
			if(msg.buff[0] == SECOND_SLAVE_MODE){
				if(!player.alive){
					BEFORE(BIG_DEADLINE, &player, startMusic, 0);
				}
				return;
			}else if(msg.buff[0] == 0){
				BEFORE(BIG_DEADLINE, &player, stopMusic, 0);
				return;
			}
		}
		if(msg.msgId == 1){
			BEFORE(BIG_DEADLINE,&tonGen, raiseVol, 0);
			return;
		}
		if(msg.msgId == 2){
			BEFORE(BIG_DEADLINE,&tonGen, lowerVol, 0);
			return;
		}
		if(msg.msgId == 3){
			int tempo = (int)msg.buff[0];
			BEFORE(BIG_DEADLINE,&player, setTempo, tempo);
		}
		if(msg.msgId == 4){
			int key = (int)msg.buff[0] - 5;
			BEFORE(BIG_DEADLINE,&player, setKey, key);
		}
	}	
}

void reader(App *self, int c) {
	if(c == 'n'){
		self->buf[self->count] = '\0';
		self->count = 0;
		int n = atoi(self->buf);
		SCI_WRITE(&sci0, "\n");
		BEFORE(BIG_DEADLINE, &player, changeStartPos, n);
	}
	if(c == 'l'){
		if(player.mode == LEADER_MODE){
			player.mode = SLAVE_MODE;
		}
		else{
			player.mode = LEADER_MODE;
		}
		return;
	}
	if(1){			
		CANMsg msg;
		msg.nodeId = LEADER_MODE;
		msg.length = 2;
		if(c == 's'){
			if(player.mode == LEADER_MODE){
				if(player.alive){
					msg.msgId = 0;
					CAN_SEND(&can0, &msg);
				}
				BEFORE(BIG_DEADLINE, &player, toggleMusic, 0);				
			}
			return;
		}
		if(c == 'q'){
			msg.msgId = 1;
			if(player.mode == LEADER_MODE){
				BEFORE(BIG_DEADLINE,&tonGen, raiseVol, 0);
				CAN_SEND(&can0, &msg);
			}
			return;
		}
		if(c == 'a'){
			msg.msgId = 2;			
			if(player.mode == LEADER_MODE){
				BEFORE(BIG_DEADLINE,&tonGen, lowerVol, 0);
				CAN_SEND(&can0, &msg);
			}
			return;
		}
		if(c == 't'){
			self->buf[self->count] = '\0';
			self->count = 0;
			int tempo = atoi(self->buf);
			SCI_WRITE(&sci0, "\n");
			if(player.mode == LEADER_MODE){
				BEFORE(BIG_DEADLINE,&player, setTempoBuf, tempo);
			}
			
		}
		else if(c == 'k'){
			msg.msgId = 4;
			self->buf[self->count] = '\0';
			self->count = 0;
			int key = atoi(self->buf);
			SCI_WRITE(&sci0, "\n");
			msg.buff[0] = (unsigned char)(key+5);
			msg.buff[1] = 0;			
			if(player.mode == LEADER_MODE){
				CAN_SEND(&can0, &msg);
				BEFORE(BIG_DEADLINE,&player, setKey, key);
			}
		}
		else{
			self->buf[self->count] = (char)c;
			self->count++;
			SCI_WRITECHAR(&sci0, c);
		}
	}
}

void startApp(App *self, int arg) {
    CANMsg msg;

    CAN_INIT(&can0);
    SCI_INIT(&sci0);
    //SCI_WRITE(&sci0, "Hello, hello...\n");	
	/*
    msg.msgId = 1;
    msg.nodeId = 1;
    msg.length = 6;
    msg.buff[0] = 'H';
    msg.buff[1] = 'e';
    msg.buff[2] = 'l';
    msg.buff[3] = 'l';
    msg.buff[4] = 'o';
    msg.buff[5] = 0;
    CAN_SEND(&can0, &msg);
	 */
}

int main() {
    INSTALL(&sci0, sci_interrupt, SCI_IRQ0);
	INSTALL(&can0, can_interrupt, CAN_IRQ0);
    TINYTIMBER(&app, startApp, 0);
    return 0;
}
