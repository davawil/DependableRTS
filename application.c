#include "TinyTimber.h"
#include "sciTinyTimber.h"
#include "canTinyTimber.h"
#include "ToneGen.h"
#include "sioTinyTimber.h"
#include "MusicPlayer.h"
#include <stdlib.h>
#include <stdio.h>
#include <math.h>

#define APP_BUFSIZE 10

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
	Time tap_sample; //timestamp for click event
	int userState; //config state, trigger handler either on pressed (0) or released (1)
	int buttonHold; //button hold and press
	char buff[APP_BUFSIZE];
} App;


typedef struct {
	Object super;
	int period;
	int intensity;
	int waveform; //0 = square, 1= sawtooth, sinus = 2
	int amplitude;
	int sample;
	int param;
} Modulator;


#define PI 3.14159265

//For the modulator
#define MAX_I_LFO 100
#define MAX_F_LFO 200 //period msec
#define MIN_I_LFO 0
#define MIN_F_LFO 5000 //period msec

#define MAX_LFO_VOLUME 100
#define MAX_LFO_TEMPO 50
#define MAX_LFO_PERIOD 25

/*
#define MOD_VOL 1
#define MOD_TEMPO 2
#define MOD_PERIOD 3
 * */

//The different waveforms
#define MOD_OFF 0
#define SQUARE 1
#define SAWTOOTH 2
#define SINUS 3


#ifndef LEADER
App app = { initObject(), 0, 'X', 0 , 120, 0, 0 , 0, 0, initTimer(), 0,0,0,0};
#else
App app = { initObject(), 0, 'X', 0, 0,  initTimer(), 0, 0, 0};
#endif

Modulator modulator = {initObject(),MIN_F_LFO,MAX_I_LFO,MOD_OFF, MOD_VOL};

void reader(App*, int);
void receiver(App*, int);
void button_pressed(App *, int);
void keyPrinter(int);

Serial sci0 = initSerial(SCI_PORT0, &app, reader);

Can can0 = initCan(CAN_PORT0, &app, receiver);

ToneGen toneGen = {initObject(),5,TONE_DEADLINE,0,0,0,5,0};

MusicPlayer player = {initObject(), 120, 0, 0,0, 120};

SysIO io = initSysIO(SIO_PORT0, &app, button_pressed);

/*SYSIO*/
void set_LED(MusicPlayer *self, int unused){
	if(self->enabled){
		int offset = BPM_TO_MSEC(self->tempo)/2;
		SIO_TOGGLE(&io);
		AFTER(MSEC(offset), self, set_LED, unused);
	}
}
void wave_square(Modulator *self, int unused){
	if(self->waveform == SQUARE){
		if(self->param == MOD_VOL){
			int vol = SYNC(&toneGen, getVol, 0);
			int modVol = vol *(1 + ((double)self->amplitude)/100);		
			SYNC(&toneGen, setModVol, modVol);
		}
		else if(self->param == MOD_TEMPO){
			int tempo = SYNC(&player, get_tempo, 0);
			int modTempo = tempo *(1 + ((double)self->amplitude)/100);
			DEBUG_INT(modTempo);
			DEBUG(self->amplitude);
			SYNC(&player, set_mod_tempo, modTempo);
		}
		AFTER(MSEC(self->period/2),self, wave_square, 0);
		self->amplitude = -self->amplitude;
	}
}

void wave_saw(Modulator *self, int value){
	if(self->waveform == SAWTOOTH){
		if(self->param == MOD_VOL){
			int vol = SYNC(&toneGen, getVol, 0);
			int modVol = vol *(1 + ((double)self->amplitude)/100);		
			SYNC(&toneGen, setModVol, modVol);
		}
		
		
		AFTER(MSEC(self->period/15),self, wave_saw, 0);
		int step =  2*self->intensity/15;
	
		int amplitude;
		if(self->sample < 15) {
			amplitude = self->amplitude + step;
		}
		else {
			amplitude = self->amplitude - step;
		}

		self->sample = (self->sample + 1) % (30);
		self->amplitude = amplitude;
	}
}

void set_mod_param(Modulator *self, int value){
	self->param = value;
	if(value == MOD_TEMPO){
		if(self->intensity > MAX_I_LFO/2)
			self->intensity = MAX_I_LFO/2;
	}
	SYNC(&toneGen, setModulated, value);
	SYNC(&player, set_modulated, value);
	
}

void wave_sine(Modulator *self, int value){
	if(self->waveform == SINUS){
		if(self->param == MOD_VOL){
			int vol = SYNC(&toneGen, getVol, 0);
			int modVol = vol *(1 + ((double)self->amplitude)/100);		
			SYNC(&toneGen, setModVol, modVol);
		}		
		
		AFTER(MSEC(self->period/16),self, wave_sine, 0);
	
		int amplitude = self->intensity * sin((double)self->sample/16*2*PI);

		self->sample = (self->sample + 1) % (16);
		self->amplitude = amplitude;
	}
}

void set_mod_period(Modulator *self, int value){
	if(value >= MAX_F_LFO || value <= MIN_F_LFO) {
		self->period = value;
	}
}

void set_mod_intensity(Modulator *self, int value){
	self->intensity = value;
	if(self->param == MOD_TEMPO){
		if(self->intensity > MAX_I_LFO/2)
			self->intensity = MAX_I_LFO/2;
	}
	if(self->waveform == SQUARE){
		self->amplitude = self->intensity;
	}
	if(self->waveform == SAWTOOTH){
		self->amplitude = -self->intensity;
		self->sample = 0;
	}
	if(self->waveform == SINUS){
		self->amplitude = 0;
		self->sample = 0;
	}
	
}

void set_mod_waveform(Modulator *self, int value){
	if(self->waveform != value){
		
		self->waveform = value;
		
		if(value == SQUARE){
			self->amplitude = self->intensity;
			wave_square(self, 0);
		}
		
		if(value == SAWTOOTH){
			self->amplitude = -self->intensity;
			self->sample=0;
			wave_saw(self, 0);
		}
		
		if(value == SINUS){
			self->amplitude = 0;
			self->sample=0;
			wave_sine(self, 0);
		}
	}
}


int get_mod_amplitude(Modulator *self, int unused){
	return self->amplitude;
}

void reset_bounce(App *self, int unused){
	self->contactBounce = 0;
}

void press_and_hold(App *self,int unused) {
	Time diff = T_SAMPLE(&self->timer) - self->tap_sample;
	
	if(self->buttonHold == 1 && diff >= SEC(2)) {
		DEBUG("Entered press-and-hold mode \n");
	}
}

void button_pressed(App *self, int unused){
	
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
	
	if(!self->contactBounce){

		//if pressed, react to release
		if(state == 0) {
			self->contactBounce = 1;		
			AFTER(MSEC(100), self, reset_bounce, 0);
		}
		
		//If button is pressed the first time, set intital tap sample
		if(self->tap_sample == 0 && state == 0){
			self->tap_sample = T_SAMPLE(&self->timer);
			
			//START TIMER 2 SEC
			self->buttonHold = 1;
			AFTER(MSEC(2000), self, press_and_hold, 0);
		}
		//Button is realeasd or tapped the second time
		else{
			//measure time
			Time diff = T_SAMPLE(&self->timer) - self->tap_sample;
			int time = SEC_OF(diff) * 1000 + MSEC_OF(diff);

			//button is pressed second time
			if(state == 0){
				if(diff < SEC(2) && diff > MSEC(200)) {
					
					DEBUG("Inter-arrival time: ");
					DEBUG_INT(time);
					DEBUG("\n");
					
					ASYNC(&player, set_tempo, MSEC_TO_BPM(time));
				}
				else {
					DEBUG("invalid\n");
				}
				self->tap_sample = 0;
				T_RESET(&self->timer);
			}
			//button is realeased
			else if(state == 1) {
				if(diff > SEC(2)) {
					DEBUG("Time held (msec): ");
					DEBUG_INT(time);
					DEBUG("\n");
					
					//print default tempo
					DEBUG("Default tempo: ");
					DEBUG_INT(120);
					DEBUG("\n");
					
					//set default tempo
					ASYNC(&player, set_tempo, 120);
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
	char string[20];
	
    CANMsg msg;
    CAN_RECEIVE(&can0, &msg);
	snprintf(string, 20 , "%d %d %d %d", msg.buff[0], (signed char)msg.buff[1], msg.buff[2], msg.buff[3]);
	
#ifndef LEADER
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
	
#endif

    SCI_WRITE(&sci0, "Can msg received: ");
	SCI_WRITE(&sci0, string);
   // SCI_WRITE(&sci0, msg.buff);
	SCI_WRITE(&sci0, "\n");
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
	//raise volume
	else if(c == 'w'){
			SYNC(&toneGen, raiseVol, 0);
	}
	//lower volume
	else if(c == 's'){
			SYNC(&toneGen, lowerVol, 0);
	}
	#endif
	//mute
	else if(c == 'm'){
		if(!SYNC(&toneGen, getMuted, 0)){
			SYNC(&toneGen, setMuted, 1);
		}
		else{
			SYNC(&toneGen, setMuted, 0);
		}
	}
	//modulator intensity
	else if(c == 'x'){
		int intensity = atoi(self->buff);
		//clear buffer
		for(int i = 0; i < APP_BUFSIZE; i++){
			self->buff[i] = 0;
		}
		self->count = 0;
		DEBUG("Modulator intensity: ");
		DEBUG_INT(intensity);
		SYNC(&modulator, set_mod_intensity, intensity); 
	}
	//modulator frequency
	else if(c == 'z'){
		int period = atoi(self->buff);
		//clear buffer
		for(int i = 0; i < APP_BUFSIZE; i++){
			self->buff[i] = 0;
		}
		self->count = 0;
		DEBUG("Modulator period: ");
		DEBUG_INT(period);
		SYNC(&modulator, set_mod_period, period); 
	}
	
	//modulator waveform
	else if(c == 'y'){
		int waveform = atoi(self->buff);
		//clear buffer
		for(int i = 0; i < APP_BUFSIZE; i++){
			self->buff[i] = 0;
		}
		self->count = 0;

		//check if valid waveform
		if(waveform == SQUARE || waveform == SAWTOOTH || waveform == SINUS) {
			DEBUG("Modulator waveform: ");
			DEBUG_INT(waveform);
			SYNC(&modulator, set_mod_waveform, waveform);
		}
		else {
			DEBUG("Invalid waveform");
		}
	}
	else if(c == 'n'){
		int param = atoi(self->buff);
		//clear buffer
		for(int i = 0; i < APP_BUFSIZE; i++){
			self->buff[i] = 0;
		}
		self->count = 0;

		//check if valid waveform
		if(param == MOD_VOL || param == MOD_PERIOD || param == MOD_TEMPO || param == MOD_OFF) {
			DEBUG("Modulate paramter: ");
			DEBUG_INT(param);
			SYNC(&modulator, set_mod_param, param);
		}
		else {
			DEBUG("Invalid parameter");
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
    CAN_SEND(&can0, &msg);
	
	SIO_WRITE(&io, 1);
}

int main() {
    INSTALL(&sci0, sci_interrupt, SCI_IRQ0);
	INSTALL(&can0, can_interrupt, CAN_IRQ0);
	INSTALL(&io, sio_interrupt, SIO_IRQ0);
    TINYTIMBER(&app, startApp, 0);
    return 0;
}
