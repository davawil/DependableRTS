#include "TinyTimber.h"
#include "sciTinyTimber.h"
#include "canTinyTimber.h"
#include "ToneGen.h"
#include "sioTinyTimber.h"
#include "MusicPlayer.h"
#include <stdlib.h>
#include <stdio.h>

#define APP_BUFSIZE 10
#define CAN_QUEUE_SIZE 10


#define CAN_ENCODE_PLAYER_STATE(tempo, key, index, enabled) {(uchar)tempo, (uchar)key, (uchar)index, (uchar)enabled, 0, 0, 0, 0}

#define MSG_CHANGE_PARAM 2
#define MSG_RESTART 3

#define LEADER

#define TONE_DEADLINE USEC(100)
#define BIG_DEADLINE MSEC(20)

#define DEBUG(x) snprintf(debugMessage, 40 , x); SCI_WRITE(&sci0, debugMessage);
#define DEBUG_INT(x) snprintf(debugMessage, 40 , "%d\n", x); SCI_WRITE(&sci0, debugMessage);

char debugMessage[40];

typedef struct {
	CANMsg can;
	uchar padding;
} CANMsgElement;

typedef struct {
	Object super;
	char canBuffLast;
	char canBuffFirst;
	char canBuffCount;
	char canFetching;
	CANMsgElement canBuff[CAN_QUEUE_SIZE];	
} CANRegulator;

typedef struct {
    Object super;
    int count;
    char c;
	int sum;
#ifndef LEADER
	uchar ghost_tempo;
	int ghost_key;
	uchar ghost_index;
	uchar ghost_enabled;
#endif
	uchar contactBounce;
	Timer timer;
	Time tap_sample; 			//timestamp for click event
	int userState; 				//config state, trigger handler either on pressed (0) or released (1)
	int buttonHold; 			//button hold and press
	int sequenceNumber;
	char buff[APP_BUFSIZE];
} App;

#define initCANRegulator()  { initObject(), 0, 0, 0, 0}

#ifndef LEADER
App app = { initObject(), 0, 'X', 0 , 120, 0, 0 , 0, 0, initTimer(), 0,0,0,0};
#else
App app = { initObject(), 0, 'X', 0, 0,  initTimer(), 0,0,0,0};
#endif

void reader(App*, int);
void receiver(App*, int);
void button_pressed(App *, int);
void keyPrinter(int);
void handleMessage(App *self, CANMsg* msgptr);

Serial sci0 = initSerial(SCI_PORT0, &app, reader);

Can can0 = initCan(CAN_PORT0, &app, receiver);

ToneGen toneGen = {initObject(),5,TONE_DEADLINE,0,0,0};

MusicPlayer player = {initObject(), 120, 0, 0,0};

SysIO io = initSysIO(SIO_PORT0, &app, button_pressed);

CANRegulator regulator = initCANRegulator();

/*SYSIO*/
void set_LED(MusicPlayer *self, int unused){
	if(self->enabled){
		int offset = BPM_TO_MSEC(self->tempo)/2;
		SIO_TOGGLE(&io);
		AFTER(MSEC(offset), self, set_LED, unused);
	}
}

void pushCanQueue(CANRegulator *self, CANMsg *msg){
	CANMsgElement element = {*msg, 0};
	//Add message to buffer
	self->canBuff[(int)(self->canBuffLast)] = element;
	//update buffer pointer
	self->canBuffLast = (self->canBuffLast + 1) % CAN_QUEUE_SIZE;		
	//update number of messages in buffer
	self->canBuffCount = self->canBuffCount + 1;		
}

void popCanQueue(CANRegulator *self, CANMsg *msg){
	//get next message from buffer
	CANMsgElement msgelem = self->canBuff[(int)(self->canBuffFirst)];
	*msg = msgelem.can;
	//update pointer and number of messages in buffer
	self->canBuffFirst = (self->canBuffFirst +1) % CAN_QUEUE_SIZE;
	self->canBuffCount = self->canBuffCount - 1;
}

void fetchCanQueue(CANRegulator *self, int unused){
	if(self->canBuffCount > 0) {
		self->canFetching = 1;
		//get next message from buffer	
		CANMsg msg;
		popCanQueue(self, &msg);
		
		//handle message 
		SYNC(&app,handleMessage,&msg);
		
		//fetch next message after 1 sec
		AFTER(SEC(1),self,fetchCanQueue,0);
	}
	else {
		//if buffer empty, indicate termination of fetching
		self->canFetching = 0;
	}
}

void handleMessage(App *self, CANMsg* msgptr){
	//get current timestamp
	Time time = T_SAMPLE(&self->timer);
	int timestamp = SEC_OF(time);
	
	//print the message
	DEBUG("Received sequence number: ");
	DEBUG_INT(msgptr->msgId);
	DEBUG("Timestamp: ");
	DEBUG_INT(timestamp);
	DEBUG("\n");

}


int getCanBuffCount(CANRegulator *self, int unused){
	return self->canBuffCount;
}

int getCanFetching(CANRegulator *self, int unused){
	return self->canFetching;
}

void reset_bounce(App *self, int unused){
	self->contactBounce = 0;
}

void burst_transmission(App *self,int unused) {
	Time diff = T_SAMPLE(&self->timer) - self->tap_sample;
	
	//send burst if buttonhold and button held more than 2 seconds
	if(self->buttonHold == 1 && diff >= SEC(2)) {		
		//Send can message
		CANMsg msg;
		msg.msgId = self->sequenceNumber;
		msg.nodeId = 0;
		CAN_SEND(&can0, &msg);
		
		//send next burst
		AFTER(MSEC(500),self,burst_transmission,0);
		
		self->sequenceNumber = (self->sequenceNumber + 1) % 127;
	}
}

void press_and_hold(App *self,int unused) {
	Time diff = T_SAMPLE(&self->timer) - self->tap_sample;
	
	if(self->buttonHold == 1 && diff >= SEC(2)) {
		DEBUG("Entered press-and-hold mode \n");
		ASYNC(self,burst_transmission,0);
	}
}

void button_pressed(App *self, int unused){
	
	//read current state
	int state = SIO_READ(&io);
		
	//if pressed, react to release
	if(state == 0) {
		SIO_TRIG(&io, 1);
		self->userState = 1;
	}
			
	//if released, react to press
	if(state == 1) {
		SIO_TRIG(&io, 0);
		self->userState = 0;
	}
	
	//valid button press
	if(!self->contactBounce){
	
		//if pressed, set contactbounce
		if(state == 0) {
			self->contactBounce = 1;		
			AFTER(MSEC(100), self, reset_bounce, 0);
		}
		
		//Send can msg if button pressed
		if(state == 0) {
			CANMsg msg;
			msg.msgId = self->sequenceNumber;
			msg.nodeId = 0;
			CAN_SEND(&can0, &msg);
			
			//print sequence number
			DEBUG("Sequence number: ");
			DEBUG_INT(self->sequenceNumber);
			DEBUG("\n");
			
			self->sequenceNumber = (self->sequenceNumber + 1) % 127;
		}

		
		//If button is pressed the first time, set intital tap sample
		if(self->tap_sample == 0 && state == 0){
			self->tap_sample = T_SAMPLE(&self->timer);
			
			//START TIMER 2 SEC
			self->buttonHold = 1;
			AFTER(MSEC(2000), self, press_and_hold, 0);
		}
		//Button is realeasd or tapped the second time
		//this code below is mainly for problem 1
		else{
			//measure time
			Time diff = T_SAMPLE(&self->timer) - self->tap_sample;
			int time = SEC_OF(diff) * 1000 + MSEC_OF(diff);

			//button is pressed second time
			if(state == 0){
				
				//START TIMER 2 SEC(only for problem 2)
				self->buttonHold = 1;
				AFTER(MSEC(2000), self, press_and_hold, 0);
				
				//if(diff < SEC(2) && diff > MSEC(200)) {
					
				//	DEBUG("Inter-arrival time: ");
				//	DEBUG_INT(time);
				//	DEBUG("\n");
					
				//ASYNC(&player, set_tempo, MSEC_TO_BPM(time));
				//}
				
				//reset tap sample
				self->tap_sample = 0;
				T_RESET(&self->timer);
			}
			//button is realeased
			else if(state == 1) {
				if(diff > SEC(2)) {
					/*
					DEBUG("Time held (msec): ");
					DEBUG_INT(time);
					DEBUG("\n");
					
					//print default tempo
					DEBUG("Default tempo: ");
					DEBUG_INT(120);
					DEBUG("\n");
					*/
					//set default tempo
				//	ASYNC(&player, set_tempo, 120);
					self->tap_sample = 0;
					T_RESET(&self->timer);
				}
				//button no longer held
				self->buttonHold = 0;
			}
		}
	}
	//if contact bounce invert buttonHold
	//since every contact bounce will have a release AND a press it will reset to old value
	//needed to reset hold when click is too quick
	else {
		if(self->buttonHold == 1) {self->buttonHold = 0;}
		else {self->buttonHold = 1;}
	}
}



void receiver(App *self, int unused) {
    CANMsg msg;
    CAN_RECEIVE(&can0, &msg);
	//CANMsgElement element = {msg, 0};
	
	int buffcount = SYNC(&regulator, getCanBuffCount, 0);
	if(buffcount < CAN_QUEUE_SIZE) {
		//start fetching from queue, if queue-fetching is not already running
		SYNC(&regulator, pushCanQueue, &msg);
		if(buffcount == 0 && SYNC(&regulator, getCanFetching, 0) == 0) {
			ASYNC(&regulator, fetchCanQueue, 0);
		}
	}
	
#ifndef LEADER

/*
	if(msg.msgId == MSG_CHANGE_PARAM || msg.msgId == MSG_RESTART ){
		uchar old_enabled = SYNC(&player, get_player_enabled, 0);
		//update all parameters
		SYNC(&player, set_tempo, msg.buff[0]);
		SYNC(&player, set_key, msg.buff[1] - 5);
		SYNC(&player, set_player_enabled, msg.buff[3]);
		if(msg.msgId == MSG_RESTART)
			SYNC(&player, set_index, msg.buff[2]);
		
		
		//if leader is enabled but slave is not, then slave must start playing
		if(msg.buff[3] && !old_enabled)
			ASYNC(&player, play_tune, 0);
	}
	*/
#endif
}

void can_update(uchar tempo, uchar key, uchar index, uchar enabled, int msgId){
	CANMsg msg;
	msg.msgId = msgId;
    msg.nodeId = 0;
    msg.length = 4;
    msg.buff[0] = tempo;
    msg.buff[1] = (uchar)(key + 5);
    msg.buff[2] = index;
    msg.buff[3] = enabled;
    CAN_SEND(&can0, &msg);
}

void keyPrinter(int key){	
	//continue only if key is in range
	if(key > 5 || key < -5)
		return;
	
	SCI_WRITE(&sci0, "Key: ");
	char string[2];
	snprintf(string, 2, "%d", key);
	SCI_WRITE(&sci0, string);;
	SCI_WRITE(&sci0, "\n");
	//print all periods in the tune
	/*
	for(int i = 0; i < TONES_IN_TUNE; i++){
		char string[5];
		snprintf(string, 5 , "%d", tone_periods[tone_ind[i] + 10 + key]);
		SCI_WRITE(&sci0, string);
		SCI_WRITE(&sci0, " ");
	}
	SCI_WRITE(&sci0, "\n");
	*/
}

void reader(App *self, int c) {
    SCI_WRITE(&sci0, "Rcv: \'");
    SCI_WRITECHAR(&sci0, c);
    SCI_WRITE(&sci0, "\'\n");
	char string[APP_BUFSIZE];
	if(c == 'e'){
		int num = atoi(self->buff);
		//clear buffer
		for(int i = 0; i < APP_BUFSIZE; i++){
			self->buff[i] = 0;
		}
		self->count = 0;
		keyPrinter(num);
	}
#ifdef LEADER
	//play tone generator/music player
	else if(c == 'p'){
		DEBUG("play");
		if(!SYNC(&player, get_player_enabled, 0)){
			ASYNC(&player, start_player, 0);
		}
		else{
			SYNC(&player, stop_player, 0);
		}
	}
	else if(c == 'i'){
		SYNC(&player, inc_key, 0);
		snprintf(string, APP_BUFSIZE, "%d", SYNC(&player, get_key, 0));
		SCI_WRITE(&sci0, "\nkey: ");
		SCI_WRITE(&sci0, string);
		SCI_WRITE(&sci0, "\n");
	}
	else if(c == 'k'){
		SYNC(&player, dec_key, 0);
		snprintf(string, APP_BUFSIZE, "%d", SYNC(&player, get_key, 0));
		SCI_WRITE(&sci0, "\nkey: ");
		SCI_WRITE(&sci0, string);
		SCI_WRITE(&sci0, "\n");
	}
	else if(c == 't'){
		SYNC(&player, inc_tempo, 0);
		snprintf(string, APP_BUFSIZE, "%d", SYNC(&player, get_tempo, 0));
		SCI_WRITE(&sci0, "\ntempo: ");
		SCI_WRITE(&sci0, string);
		SCI_WRITE(&sci0, "\n");
	}
	else if(c == 'q'){
		int tempo = atoi(self->buff);
		//clear buffer
		for(int i = 0; i < APP_BUFSIZE; i++){
			self->buff[i] = 0;
		}
		self->count = 0;
		SYNC(&player, set_tempo, tempo); 
	}
	else if(c == 'g'){
		SYNC(&player, dec_tempo, 0);
		snprintf(string, APP_BUFSIZE, "%d", SYNC(&player, get_tempo, 0));
		SCI_WRITE(&sci0, "\ntempo: ");
		SCI_WRITE(&sci0, string);
		SCI_WRITE(&sci0, "\n");
	}
/*	
#else

	//emulates master while in slave mode
	else if(c == 'p'){
		if(!self->ghost_enabled){
			self->ghost_enabled = 1;
		}
		else{
			self->ghost_enabled = 0;
		}
		can_update(self->ghost_tempo, self->ghost_key, self->ghost_index, self->ghost_enabled, MSG_RESTART);
	}
	else if(c == 'i'){
		if(self->ghost_key < KEY_MAX)
			self->ghost_key++;
		char s[APP_BUFSIZE];
		snprintf(s, APP_BUFSIZE, "%d", self->ghost_key);
		SCI_WRITE(&sci0, "\nkey: ");
		SCI_WRITE(&sci0, s);
		SCI_WRITE(&sci0, "\n");	
		can_update(self->ghost_tempo, self->ghost_key, self->ghost_index, self->ghost_enabled, MSG_CHANGE_PARAM);
	}
	else if(c == 'k'){
		if(self->ghost_key > KEY_MIN)
			self->ghost_key--;
			
		char s[APP_BUFSIZE];
		snprintf(s, APP_BUFSIZE, "%d", self->ghost_key);
		SCI_WRITE(&sci0, "\nkey: ");
		SCI_WRITE(&sci0, s);
		SCI_WRITE(&sci0, "\n");
			
		can_update(self->ghost_tempo, self->ghost_key, self->ghost_index, self->ghost_enabled, MSG_CHANGE_PARAM);
	}
	else if(c == 't'){
		if(self->ghost_tempo < TEMPO_MAX)
			self->ghost_tempo+=10;
		can_update(self->ghost_tempo, self->ghost_key, self->ghost_index, self->ghost_enabled, MSG_CHANGE_PARAM);
	}
	else if(c == 'g'){
		if(self->ghost_tempo > TEMPO_MIN)
			self->ghost_tempo-=10;
		can_update(self->ghost_tempo, self->ghost_key, self->ghost_index, self->ghost_enabled, MSG_CHANGE_PARAM);
	}
	 */
#endif
	//raise volume
	else if(c == 'w'){
			SYNC(&toneGen, raiseVol, 0);
	}
	//lower volume
	else if(c == 's'){
			SYNC(&toneGen, lowerVol, 0);
	}
	//mute
	else if(c == 'm'){
		if(!SYNC(&toneGen, getMuted, 0)){
			SYNC(&toneGen, setMuted, 1);
		}
		else{
			SYNC(&toneGen, setMuted, 0);
		}
	}
	else if(c == 'd'){
		//SYNC(&toneGen, toggle_tone_deadline, 0);
	//	SYNC(&load, toggle_load_deadline, 0);
	}
	else{
		self->buff[self->count] = c;
		self->count++;
	}
}

void startApp(App *self, int arg) {
    CANMsg msg;

    CAN_INIT(&can0);
    SCI_INIT(&sci0);
    SCI_WRITE(&sci0, "Hello, hello...\n");
	SIO_INIT(&io);
	SIO_WRITE(&io, 1);

    msg.msgId = 1;
    msg.nodeId = 1;
    msg.length = 6;
    msg.buff[0] = 'H';
    msg.buff[1] = 'e';
    msg.buff[2] = 'l';
    msg.buff[3] = 'l';
    msg.buff[4] = 'o';
    msg.buff[5] = 0;
  //  CAN_SEND(&can0, &msg);
	
	SIO_WRITE(&io, 1);
}

int main() {
    INSTALL(&sci0, sci_interrupt, SCI_IRQ0);
	INSTALL(&can0, can_interrupt, CAN_IRQ0);
	INSTALL(&io, sio_interrupt, SIO_IRQ0);
    TINYTIMBER(&app, startApp, 0);
    return 0;
}
