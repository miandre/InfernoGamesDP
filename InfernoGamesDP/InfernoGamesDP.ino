/*
 Name:		InfernoGamesDP.ino
 Created:	4/14/2019 9:12:33 PM
 Author:	Micke
*/


#include <Wire.h>
#include <MicroLCD.h>
#include <Adafruit_NeoPixel.h>
#include "IOFunctions.h"

#include <Adafruit_FONA.h>
#include <SoftwareSerial.h>
#include "InfernoGamesDP.h"

/**************************DP-ID****************************************/
#define ID 4
/*******************************************************************/

#define DEFAULT_GAME_TIME 2
#define READY_TIME 3

#define LED_POWER_HIGH 255
#define LED_POWER_LOW 100

#define FONA_RX 4
#define FONA_TX 5
#define FONA_RST 6

#define RED 0
#define BLUE 1
#define YELLOW 3
#define NUMBER_OF_TEAMS 2

#define NO_TEAM 99
#define BEARS 0
#define STF 1
#define NA 3

#define LED_COLOR_RED pixels.Color(LED_POWER_LOW, 0, 0)
#define LED_COLOR_BLUE pixels.Color(0, 0, LED_POWER_LOW)
#define LED_COLOR_WHITE pixels.Color(255, 255, 255)
#define LED_COLOR_WHITE_LOW pixels.Color(25, 25, 25)

#define BUTTON_LED_PIN 9

#define APN "halebop.telia.se"

#define DEBOUNCE 10 
#define NUMBUTTONS sizeof(buttons)

// Which pin on the Arduino is connected to the NeoPixels?
#define NEO_PIN 12 
// How many NeoPixels are attached to the Arduino?
#define NUMPIXELS 8 

Adafruit_NeoPixel pixels(NUMPIXELS, NEO_PIN, NEO_GRB + NEO_KHZ800);

// Declaration for an SSD1306 display connected to I2C (SDA, SCL pins)
LCD_SSD1306 lcd;

// Button helpers
byte buttons[] = { A0, A1, A2, A3 };
byte pressed[NUMBUTTONS], justpressed[NUMBUTTONS], justreleased[NUMBUTTONS];
byte previous_keystate[NUMBUTTONS], current_keystate[NUMBUTTONS];

void initFONA(boolean);
void trySendData(const String&, int8_t, boolean);
void setStatus(uint8_t, uint8_t);
void setAlive(boolean);
void reportGameStart();
void reportGameEnd(boolean);
void reInitGPRS();
boolean sendData(const String&);

enum State {
	STANDBY,
	READY,
	NEUTRAL,
	TAKEN,
	END,
	KILLED
};

struct TIME
{
	int8_t seconds;
	int8_t minutes;
	int8_t hours;
};

SoftwareSerial fonaSs = SoftwareSerial(FONA_TX, FONA_RX);
SoftwareSerial* fonaSerial = &fonaSs;
Adafruit_FONA fona = Adafruit_FONA(FONA_RST);
boolean fonaInitialized = false;

State state;
byte currentTeam;
boolean standByModeIsSet = false;
boolean readyModeSet = false;
boolean neutralModeSet = false;
boolean goOnlineTimeIsSet = false;
boolean endModeSet = false;
boolean resetState = false;
boolean resetReported = true;
char PIN[5] = "1234";
const String URL_BASE = "http://www.geeks.terminalprospect.com/AIR/";
uint16_t score[NUMBER_OF_TEAMS];
TIME startTime;
TIME goOnline;
TIME stopTime;
uint32_t loopCounter = 0;
char fonaInBuffer[64];

#define FS(x) (__FlashStringHelper*)(x)
const char stf[]  PROGMEM = { "STF" };
const char bears[]  PROGMEM = { "BEARS" };
const char noTeam[]  PROGMEM = { "NOTEAM" };

const char PROGMEM red[] = { "RED" };
const char PROGMEM blue[] = { "BLUE" };
const char PROGMEM black[] = { "BLACK" };;

const char PROGMEM initializing[] = { "Initializing" };
const char PROGMEM simOk[] = { "SIM OK" };
const char PROGMEM gsmFound[] = { "GSM module found" };
const char PROGMEM networkFound[] = { "Network found" };

const char PROGMEM statusURL[] = { "UpdateStatus.php?ID=" };
const char PROGMEM teamQuery[] = { "&TEAM=" };
const char PROGMEM statusQuery[] = { "&STATUS=" };
const char PROGMEM watchdogURL[] = { "watchDog.php?ID=" };
const char PROGMEM startURL[] = { "StartGame.php?ID=" };
const char PROGMEM stopURL[] = { "StopGame.php?ID=" };

const char PROGMEM bearsQuery[] = { "&BEARS=" };
const char PROGMEM stfQuery[] = { "&STF=" };

const char PROGMEM winner[] = { "WINNER" };
const char PROGMEM scoreText[] = { "Score:   " };
const char PROGMEM noWinner[] = { "NO WINNER" };
const char PROGMEM neutralizing[] = { "NEUTRALIZING" };
const char PROGMEM neutralized[] = { "NEUTRALIZED" };
const char PROGMEM capturing[] = { " capturing!" };
const char PROGMEM transmitting[] = { "Transmitting status" };
const char PROGMEM standBy[] = { "Please stand by" };
const char PROGMEM onlineIn[] = { "Online in " };
const char PROGMEM minutesText[] = { " minutes." };
const char PROGMEM minuteText[] = { " minute." };
const char PROGMEM blankRow[] = { "                       " };


String globalTeamName;
String globalTeamColor;

const byte transmition[6][16] PROGMEM = {
	// 'reception, 16x8px
	{0b00000001, 0b00000010, 0b00000100, 0b11111111, 0b00000100, 0b00000010, 0b000000001,0b000000000,0b000000000,0b000000000,0b000000000,0b000000000,0b000000000,0b000000000, 0b000000000},
	{0b00000001, 0b00000010, 0b00000100, 0b11111111, 0b00000100, 0b00000010, 0b011000001,0b000000000,0b000000000,0b000000000,0b000000000,0b000000000,0b000000000,0b000000000, 0b000000000},
	{0b00000001, 0b00000010, 0b00000100, 0b11111111, 0b00000100, 0b00000010, 0b011000001,0b000000000,0b011100000,0b000000000,0b000000000,0b000000000,0b000000000,0b000000000, 0b000000000},
	{0b00000001, 0b00000010, 0b00000100, 0b11111111, 0b00000100, 0b00000010, 0b011000001,0b000000000,0b011100000,0b000000000,0b011110000,0b000000000,0b000000000,0b000000000, 0b000000000},
	{0b00000001, 0b00000010, 0b00000100, 0b11111111, 0b00000100, 0b00000010, 0b011000001,0b000000000,0b011100000,0b000000000,0b011110000,0b000000000,0b011111000,0b000000000, 0b000000000},
	{0b00000001, 0b00000010, 0b00000100, 0b11111111, 0b00000100, 0b00000010, 0b011000001,0b000000000,0b011100000,0b000000000,0b011110000,0b000000000,0b011111000,0b000000000, 0b011111100}
};

const byte logo[] PROGMEM = {
	// 'unnamed, 64x64px
	0xff, 0xff, 0x03, 0xfb, 0xfb, 0xfb, 0xfb, 0xfb, 0xfb, 0xfb, 0xfb, 0xfb, 0xfb, 0xfb, 0xfb, 0xfb,
	0xfb, 0xfb, 0xfb, 0xfb, 0xfb, 0xfb, 0xfb, 0xfb, 0xfb, 0xfb, 0xfb, 0xfb, 0xfb, 0xfb, 0x7b, 0x3b,
	0x3b, 0x3b, 0xfb, 0xfb, 0xfb, 0xfb, 0xfb, 0xfb, 0xfb, 0xfb, 0xfb, 0xfb, 0xfb, 0xfb, 0xfb, 0xfb,
	0xfb, 0xfb, 0xfb, 0xfb, 0xfb, 0xfb, 0xfb, 0xfb, 0xfb, 0xfb, 0xfb, 0xfb, 0xfb, 0x03, 0x03, 0xff,
	0xff, 0xff, 0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x7f, 0x3f, 0x1f, 0x8f, 0xe3, 0xf1, 0xfc, 0xfe,
	0xff, 0xfc, 0xf8, 0xf1, 0xc7, 0x1f, 0x3f, 0x7f, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x00, 0x00, 0xff,
	0xff, 0xff, 0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0x7f, 0x1f, 0x87, 0xc3, 0xf1, 0xf8, 0xfc, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xfe, 0xf8, 0xe1, 0xc3, 0x8f, 0x1f, 0x7f, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x00, 0x00, 0xff,
	0xff, 0xff, 0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x3f, 0x1f,
	0x87, 0xe3, 0xf9, 0xfc, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xfe, 0xf8, 0xe1, 0xc7,
	0x0f, 0x1f, 0x7f, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x00, 0x00, 0xff,
	0xff, 0xff, 0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xcf, 0xe7, 0xf8, 0xfc, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xfe, 0xfe, 0xfc, 0xf3, 0xe7, 0xdf, 0xbf, 0xff, 0xff, 0xff, 0xff, 0xff, 0x00, 0x00, 0xff,
	0xff, 0xff, 0x00, 0xff, 0xff, 0xff, 0xff, 0x7d, 0x79, 0x79, 0x79, 0xf9, 0xf9, 0xf9, 0xf9, 0xf9,
	0xf9, 0xf9, 0xf9, 0xf9, 0xf9, 0xf9, 0xf9, 0x79, 0x39, 0x39, 0x39, 0x79, 0xf9, 0xf9, 0xf9, 0xf9,
	0xf9, 0xf9, 0xf9, 0xf9, 0xf9, 0xf9, 0xf9, 0xf9, 0xf9, 0xf9, 0xf9, 0xf9, 0xfd, 0xf9, 0xfd, 0xfd,
	0xfd, 0xfd, 0xfd, 0xfd, 0xfd, 0xfd, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x00, 0x00, 0xff,
	0xff, 0xff, 0x00, 0xff, 0xff, 0xff, 0x7e, 0x3e, 0x3e, 0x00, 0x7e, 0x7e, 0x3d, 0x3d, 0x01, 0x31,
	0x79, 0x3d, 0x01, 0x03, 0x3f, 0x39, 0x39, 0x00, 0x01, 0x39, 0xfe, 0xfe, 0xc3, 0x81, 0x21, 0x21,
	0x21, 0x81, 0xe7, 0x39, 0x3d, 0x01, 0x01, 0x39, 0xf9, 0xf1, 0xff, 0x3d, 0x01, 0x31, 0x79, 0x3d,
	0x01, 0x03, 0x3f, 0xc7, 0x01, 0x39, 0x39, 0x39, 0x81, 0xc3, 0xff, 0xff, 0xff, 0x00, 0x00, 0xff,
	0xff, 0xff, 0x80, 0x9f, 0x9f, 0x9f, 0x9f, 0x9f, 0x9f, 0x9f, 0x9f, 0x9f, 0x9f, 0x9f, 0x9f, 0x9f,
	0x9f, 0x9f, 0x9f, 0x9f, 0x9f, 0x9f, 0x9f, 0x9f, 0x9f, 0x9f, 0x9f, 0x9f, 0x9f, 0x9f, 0x9f, 0x9f,
	0x9f, 0x9f, 0x9f, 0x9f, 0x9f, 0x9f, 0x9f, 0x9f, 0x9f, 0x9f, 0x9f, 0x9f, 0x9f, 0x9f, 0x9f, 0x9f,
	0x9f, 0x9f, 0x9f, 0x9f, 0x9f, 0x9f, 0x9f, 0x9f, 0x9f, 0x9f, 0x9f, 0x9f, 0x9f, 0x80, 0xc0, 0xff
};

char messageContent[30];

//uint16_t getTimeDiffInMinutes(struct TIME stop, struct TIME start) {
//	return getTimeDiffInSeconds(stop, start) / 60;
//}

uint16_t getTimeDiffInMinutes(struct TIME stop, struct TIME start) {
	if (stop.seconds < start.seconds) {
		--stop.minutes;
		stop.seconds += 60;
	}
	if (stop.minutes < start.minutes) {
		--stop.hours;
		stop.minutes += 60;
	}
	if (stop.hours < start.hours) {
		stop.hours += 24;
	}

	uint8_t hourDiff = stop.hours - start.hours;
	uint8_t minDiff = stop.minutes - start.minutes;
	uint8_t secDiff = stop.seconds - start.seconds;

	return  ((hourDiff * 60) + (minDiff) + (secDiff > 30 ? 1 : 0));
}

void setupDisplay() {
	lcd.begin();
	lcd.clear();
	lcd.setCursor(30, 3);
	lcd.setFontSize(FONT_SIZE_XLARGE);
}

void setupNeoPixelBar() {
	pixels.begin();
	pixels.clear();
	pixels.fill(LED_COLOR_WHITE_LOW, 0, 8);
	pixels.show();
}

void setupButtons() {
	for (byte i = 0; i < NUMBUTTONS; i++) {
		pinMode(buttons[i], INPUT_PULLUP);
		digitalWrite(buttons[i], HIGH);
	}
	pinMode(BUTTON_LED_PIN, OUTPUT);
}

void check_switches()
{
	static byte previousstate[NUMBUTTONS];
	static byte currentstate[NUMBUTTONS];
	static long lasttime;
	byte index;
	if (millis() < lasttime) {
		lasttime = millis(); // we wrapped around, lets just try again
	}

	if ((lasttime + DEBOUNCE) > millis()) {
		return; // not enough time has passed to debounce
	}
	// ok we have waited DEBOUNCE milliseconds, lets reset the timer
	lasttime = millis();

	for (index = 0; index < NUMBUTTONS; index++) {
		justpressed[index] = 0;       // when we start, we clear out the "just" indicators
		justreleased[index] = 0;

		currentstate[index] = digitalRead(buttons[index]);   // read the button
		if (currentstate[index] == previousstate[index]) {
			if ((pressed[index] == LOW) && (currentstate[index] == LOW)) {
				// just pressed
				justpressed[index] = 1;
			}
			else if ((pressed[index] == HIGH) && (currentstate[index] == HIGH)) {
				// just released
				justreleased[index] = 1;
			}
			pressed[index] = !currentstate[index];  // remember, digital HIGH means NOT pressed
		}
		previousstate[index] = currentstate[index];   // keep a running tally of the buttons
	}
}

byte getPressedButton() {
	byte thisSwitch = 255;
	check_switches();  //check the switches &amp; get the current state
	for (byte i = 0; i < NUMBUTTONS; i++) {
		current_keystate[i] = justpressed[i];
		if (current_keystate[i] != previous_keystate[i]) {
			if (current_keystate[i]) thisSwitch = i;
		}
		previous_keystate[i] = current_keystate[i];
	}
	return thisSwitch;
}

void writeCurrentTeamLogoToDisplay() {
	lcd.clear();
	lcd.setFontSize(FONT_SIZE_XLARGE);
	lcd.setCursor((128 - (globalTeamName.length() * 9)) / 2, 3);
	lcd.println(globalTeamName);
	printSignalLevelToDisplay();
}

void setGlobalTeamString(const byte & team)
{
	switch (team)
	{
	case BEARS:
		globalTeamName = FS(bears);
		break;
	case STF:
		globalTeamName = FS(stf);
		break;
	}
}

void lightUpTeamColour(byte team) {
	uint32_t ledColor;
	switch (team)
	{
	case BEARS:
		ledColor = LED_COLOR_RED;
		break;
	case STF:
		ledColor = LED_COLOR_BLUE;
		break;
	case NO_TEAM:
		ledColor = LED_COLOR_WHITE_LOW;
		break;
	}
	pixels.fill(ledColor, 0, 8);
	pixels.show();
}

byte getTeamIdFromName(const String & team) {
	if (team.equalsIgnoreCase(FS(bears))) return BEARS;
	if (team.equalsIgnoreCase(FS(stf))) return STF;
}

void setCurrentTeamColor(byte team) {
	switch (team)
	{
	case BEARS:
		globalTeamColor = FS(red);
		break;
	case STF:
		globalTeamColor = FS(blue);
		break;
	case NO_TEAM:
		globalTeamColor = FS(black);
		break;
	}
}

void writeStatusTextToDisplay(String displayText) {
	lcd.setFontSize(FONT_SIZE_SMALL);
	lcd.println(displayText);
}

void handleButtons(byte pressedButton) {

	if (pressedButton != currentTeam) {
		switch (pressedButton)
		{
		case RED:
			setTakenMode(BEARS);
			break;
		case YELLOW:
			neutralizeDP();
			break;
		case BLUE:
			setTakenMode(STF);
			break;
		}
	}
}

boolean checkForSMS(char* smsbuff) {
	if (fonaInitialized) {	
		char* bufPtr = fonaInBuffer;    //handy buffer pointer
		if (fona.available()) {     //any data available from the FONA?
			uint8_t slot = 0;            //this will be the slot number of the SMS
			uint8_t charCount = 0;

			if ((state == STANDBY) || (state == READY)) {  //Because of junk in the buffer we need 2 versions.
				uint8_t sanityCheck = 0;  // use to avoud inifinite loop
				do {
					*bufPtr = fona.read();
					Serial.write(*bufPtr);
				} while ((*bufPtr != '+') && (++sanityCheck < 255));

				while ((*bufPtr++ != '\n') && (fona.available()) && (++charCount < (sizeof(fonaInBuffer) - 1))) {
					*bufPtr = fona.read();
					Serial.write(*bufPtr);
					delay(1);
				}
			}
			else {
				do {
					*bufPtr = fona.read();
					Serial.write(*bufPtr);
					delay(1);
				} while ((*bufPtr++ != '\n') && (fona.available()) && (++charCount < (sizeof(fonaInBuffer) - 1)));
			}

			//Add a terminal NULL to the notification string
			*bufPtr = 0;

			uint16_t smslen;

			//Scan the notification string for an SMS received notification.
			//  If it's an SMS message, we'll get the slot number in 'slot'
			if (1 == sscanf(fonaInBuffer, "+CMTI: \"SM\",%d", &slot)) {
				if (fona.readSMS(slot, smsbuff, 80, &smslen)) {
					fona.deleteSMS(slot);
					return true;
				}
			}
		}
	}
	return false;
}

TIME getTime() {
	TIME currentTime;
	char buffer[23];
	fona.getTime(buffer, 23);
	String time = String(buffer);
	currentTime.hours = time.substring(10, 12).toInt();
	currentTime.minutes = time.substring(13, 15).toInt();
	currentTime.seconds = time.substring(16, 18).toInt();
	return currentTime;
}

void setGoOnlineAndStopTime(const String & onlineTime) {
	if (onlineTime.length() >= 8) {
		uint8_t hours = onlineTime.substring(0, 2).toInt();
		uint8_t minutes = onlineTime.substring(3, 5).toInt();
		uint8_t seconds = onlineTime.substring(6, 8).toInt();
		goOnline.hours = hours;
		goOnline.minutes = minutes;
		goOnline.seconds = seconds;
		setStopTime(((hours + DEFAULT_GAME_TIME)%24), minutes, seconds);
		goOnlineTimeIsSet = true;
	}
}

void setStopTime(const uint8_t & hours, const uint8_t & minutes, const uint8_t & seconds) {
	stopTime.hours = hours;
	stopTime.minutes = minutes;
	stopTime.seconds = seconds;
}

void printTransmittingInfo() {
	lcd.setCursor((128 - (sizeof(transmitting) * 5)) / 2, 6);
	lcd.print(FS(transmitting));
	lcd.setCursor((128 - (sizeof(standBy) * 5)) / 2, 7);
	lcd.print(FS(standBy));
}

void handleMessage(char* smsbuff) {
	String message = String(smsbuff);
	if (message.startsWith(F("STANDBY"))) {
		setStandbyMode(message.substring(8));
	}
	else if (message.startsWith(F("READY"))) {
		setReadyMode(message.substring(6));
	}
	else if (message.startsWith(F("NEUTRALIZE"))) {
		neutralizeDP();
	}
	else if (message.startsWith(F("NEUTRAL"))) {
		setNeutralMode(true);
	}
	else if (message.startsWith(F("TAKEN"))) {
		setTakenMode(getTeamIdFromName(message.substring(6)));
	}
	else if (message.startsWith(F("STOP"))) {
		String time = message.substring(5);
		uint8_t hours = time.substring(0, 2).toInt();
		uint8_t minutes = time.substring(3, 5).toInt();
		uint8_t seconds = time.substring(6, 8).toInt();
		setStopTime(hours, minutes, seconds);
	}
	else if (message.startsWith(F("CHECK"))) {
		setAlive(true);
	}
}

void displayTransmittingText() {
	lcd.clear();
	printSignalLevelToDisplay();
	lcd.setFontSize(FONT_SIZE_SMALL);
	lcd.setCursor((128 - ((sizeof(capturing) + 3) * 5)) / 2, 3);
	lcd.print(globalTeamName);
	lcd.print(FS(capturing));
	printTransmittingInfo();
}

void removeTransmittingText() {
	lcd.setFontSize(FONT_SIZE_SMALL);
	lcd.setCursor(0, 2);
	lcd.print(FS(blankRow));
	lcd.setCursor(0, 6);
	lcd.println(FS(blankRow));
	lcd.print(FS(blankRow));
	printSignalLevelToDisplay();
}

void setStandbyMode(const String & onlineTime) {
	state = STANDBY;
	endModeSet = false;
	standByModeIsSet = true;
	setGoOnlineAndStopTime(onlineTime);
	setStatus(NO_TEAM, 3);
	lcd.clear();
	lcd.backlight(false);
	lcd.setCursor(32, 0);
	lcd.draw(logo, 64, 64);
	pixels.clear();
	pixels.show();
	digitalWrite(BUTTON_LED_PIN, LOW);
	standByModeIsSet = true;
}

void setReadyMode(const String & onlineTime) {
	state = READY;
	endModeSet = false;
	setGoOnlineAndStopTime(onlineTime);
	setStatus(NO_TEAM, 1);
	readyModeSet = true;
	lcd.clear();
	lcd.backlight(false);
	lcd.setCursor(32, 0);
	lcd.draw(logo, 64, 64);
	pixels.clear();
	pixels.show();
	digitalWrite(BUTTON_LED_PIN, HIGH);
}

void neutralizeDP(){
	TIME time = getTime();
	uint16_t timeCaptured = getTimeDiffInMinutes(time, startTime);

	if (currentTeam != NO_TEAM) {
		score[currentTeam] += timeCaptured;
	}
	startTime = time;

	lcd.clear();
	lcd.setFontSize(FONT_SIZE_SMALL);
	lcd.setCursor((128 - ((sizeof(neutralizing) + 4) * 5)) / 2, 4);
	lcd.print(FS(neutralizing));
	printTransmittingInfo();

	state = KILLED;
	currentTeam = NO_TEAM;
	endModeSet = false;
	setStatus(NO_TEAM, 2);
	DEBUG_PRINT(F("KILLED1"));
	lcd.clear();
	lcd.setFontSize(FONT_SIZE_SMALL);
	lcd.setCursor((128 - ((sizeof(neutralized) + 4) * 5)) / 2, 4);
	lcd.print(FS(neutralized));
}

void setNeutralMode(boolean shouldResetScore) {
	state = NEUTRAL;
	currentTeam = NO_TEAM;
	endModeSet = false;
	if (shouldResetScore) {
		reportGameStart();
		resetScore();
	}
	setStatus(NO_TEAM, 2);
	lcd.backlight(true);
	lcd.clear();
	lcd.setCursor(32, 0);
	lcd.draw(logo, 64, 64);
	printSignalLevelToDisplay();
	digitalWrite(BUTTON_LED_PIN, HIGH);
	pixels.fill(LED_COLOR_WHITE_LOW, 0, 8);
	pixels.show();
}

void setTakenMode(byte team) {
	endModeSet = false;
	TIME time = getTime();

	if (currentTeam != NO_TEAM) {
		uint16_t timeCaptured = getTimeDiffInMinutes(time, startTime);
		score[currentTeam] += timeCaptured;
		startTime = time;
	}
	else {
		setStartTime(time);
	}

	currentTeam = team;
	setGlobalTeamString(currentTeam);
	state = TAKEN;
	lcd.backlight(true);
	digitalWrite(BUTTON_LED_PIN, LOW);
	lightUpTeamColour(team);
	delay(50);
	displayTransmittingText();

	setStatus(team, 2);

	writeCurrentTeamLogoToDisplay();
	printSignalLevelToDisplay();
	//removeTransmittingText();
	digitalWrite(BUTTON_LED_PIN, HIGH);
}

void setEndMode() {
	TIME time = getTime();
	uint16_t timeCaptured = getTimeDiffInMinutes(time, startTime);

	if (currentTeam != NO_TEAM) {
		score[currentTeam] += timeCaptured;
	}

	startTime = time;
	digitalWrite(BUTTON_LED_PIN, LOW);
	//setResult();
	setStatus(currentTeam, 1);
	delay(300);
	reportGameEnd(true);
	currentTeam = NO_TEAM;
	endModeSet = true;
}

void setStartTime(TIME now) {
	startTime.hours = now.hours;
	startTime.minutes = now.minutes;
	startTime.seconds = now.seconds;
}

void printSignalLevelToDisplay() {
	uint8_t n = fona.getRSSI();

	if (n == 0) {
		lcd.setCursor(0, 0);
		lcd.draw(transmition[0], 16, 8);
	}
	if ((n >= 1) && (n <= 15)) {
		lcd.setCursor(0, 0);
		lcd.draw(transmition[1], 16, 8);
	}
	if ((n >= 16) && (n <= 31)) {
		byte i = map(n, 16, 31, 2, 5);
		lcd.setCursor(0, 0);
		lcd.draw(transmition[i], 16, 8);
	}

}

void resetScore() {
	for (byte i = 0; i < NUMBER_OF_TEAMS; i++) {
		score[i] = 0;
	}
}

void setMaxScore() {
	for (byte i = 0; i < NUMBER_OF_TEAMS; i++) {
		score[i] = 65535;
	}
}
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

/***********************************************************************************
*********************************    SETUP    **************************************
************************************************************************************/
void setup() {
	Serial.begin(115200);
	delay(100);
	globalTeamName.reserve(6);
	globalTeamColor.reserve(6);
	setupDisplay();
	setupNeoPixelBar();
	setupButtons();
	digitalWrite(BUTTON_LED_PIN, HIGH);
	currentTeam = NO_TEAM;
	delay(100);
	initFONA(true);
	lcd.clear();
	lcd.setCursor(0, 0);
	printSignalLevelToDisplay();

	/*	Define Startup-State here: */
	delay(1000);
	setStandbyMode("");
	delay(1000);
	neutralizeDP();
	//setNeutralMode(true);
	/********************************/
}

/**********************************************************************************
****************************    MAIN PROGRAM    ***********************************
************************************************************************************/
void loop() {
	globalTeamName = FS(noTeam);
	setMaxScore();
	// Ugly way to reserve mem for end game string. (?)
	reportGameEnd(false);
	resetScore();
	while (1) {
		if (checkForSMS(messageContent)) {
			handleMessage(messageContent);
		}
		uint16_t timeLeft = 0;
		switch (state) {
		case STANDBY:
			if (!standByModeIsSet) {
				setStandbyMode("");
			}
			printSignalLevelToDisplay();
			if (goOnlineTimeIsSet && getTimeDiffInMinutes(goOnline, getTime()) < READY_TIME) {
				state = READY;
			}
			delay(3000);
			break;
		case READY:
			timeLeft = getTimeDiffInMinutes(goOnline, getTime());
			if (!readyModeSet && timeLeft <= READY_TIME) {
				setReadyMode("");
			}
			if (timeLeft < 1) {
				setNeutralMode(true);
				break;
			}
			lcd.clear();
			lcd.setCursor(0, 4);
			lcd.setFontSize(FONT_SIZE_SMALL);
			lcd.print(FS(onlineIn));
			lcd.printInt(timeLeft);
			lcd.print(timeLeft > 1 ? FS(minutesText) : FS(minuteText));
			printSignalLevelToDisplay();
			delay(5000);
			break;
		case NEUTRAL:
			handleButtons(getPressedButton());
			if (++loopCounter > 50000) {
				printSignalLevelToDisplay();
				if (getTimeDiffInMinutes(stopTime, getTime()) < 1)
					state = END;
				loopCounter = 0;
			}
			break;
		case TAKEN:
			handleButtons(getPressedButton());
			if (++loopCounter > 50000) {
				printSignalLevelToDisplay();
				if (getTimeDiffInMinutes(stopTime, getTime()) < 1)
					state = END;
				loopCounter = 0;
			}
			break;
		case END:
			if (!endModeSet) {
				setEndMode();
				resetScore();
			}
			setStandbyMode("");
			break;
		case KILLED:
			TIME timeKilled = getTime();
			uint16_t timeDiff = 0;
			uint8_t goTime = READY_TIME;
			DEBUG_PRINTLN(F("KILLED2"));
			while (timeDiff < goTime) {
				DEBUG_PRINTLN(timeDiff);
				lcd.setCursor(0, 4);
				lcd.setFontSize(FONT_SIZE_SMALL);
				lcd.print(FS(onlineIn));
				lcd.printInt((goTime - timeDiff));
				lcd.print((goTime - timeDiff) > 1 ? FS(minutesText) : FS(minuteText));
				printSignalLevelToDisplay();
				delay(400);
				if (checkForSMS(messageContent)) {
					handleMessage(messageContent);
				}
				timeDiff = getTimeDiffInMinutes(getTime(), timeKilled);
				DEBUG_PRINTLN(timeDiff);
				printSignalLevelToDisplay();
			}
			setNeutralMode(false);
			break;
		}
	}
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

void flushFONA() {
	flushSerial();
	while (fona.available()) {
		Serial.write(fona.read());
	}
	fona.flush();
}

void initFONA(boolean startup) {
	if (startup) {
		lcd.clear();
		lcd.setCursor(0, 0);
		writeStatusTextToDisplay(FS(initializing));
	}

	fonaSerial->begin(4800);
	if (!fona.begin(*fonaSerial)) {
		while (true);
	}
	if (startup) { writeStatusTextToDisplay(FS(gsmFound)); }

	flushFONA();

	fona.setGPRSNetworkSettings(F(APN));
	fona.setHTTPSRedirect(false);


	while (!fona.unlockSIM(PIN)) {
		delay(100);

		if (fona.getNetworkStatus() == 1)
			break;
	}
	if (startup) { writeStatusTextToDisplay(FS(simOk)); }

	flushFONA();

	delay(500);
	uint8_t counter = 0;
	while (fona.getNetworkStatus() != 1) {

		fona.flush();
		delay(500);
		counter++;
		if (counter == 5) {
			counter = 0;
		}
	}

	flushFONA();

	if (startup) { writeStatusTextToDisplay(FS(networkFound)); }
	fona.enableGPRS(false);
	delay(1000);
	while (!fona.enableGPRS(true)) {

		delay(1000);
	}
	flushFONA();
	fonaInitialized = true;

	if (startup) {

		for (int i = 0; i < 7; i++) {
			fona.deleteSMS(i);
		}
	}

	fona.enableRTC(1);

	if (startup) { writeStatusTextToDisplay(FS(gsmFound)); }

	flushFONA();
}

void reInitGPRS() {
	fona.enableGPRS(false);
	delay(200);
	fona.enableGPRS(true);
	delay(500);
}

void setStatus(uint8_t teamId, uint8_t status) {
	setCurrentTeamColor(teamId);
	const String url = URL_BASE + FS(statusURL) + ID + FS(teamQuery) + globalTeamColor + FS(statusQuery) + status;
	DEBUG_PRINTLN(url);
	trySendData(url, 2, true);
}

void setAlive(boolean tryToReboot) {
	const String url = URL_BASE + FS(watchdogURL) + ID;
	trySendData(url, 2, tryToReboot);
}

void reportGameStart() {
	const String url = URL_BASE + FS(startURL) + ID;
	DEBUG_PRINTLN(url);
	trySendData(url, 2, true);
}

void reportGameEnd(boolean transmit) {
	const String url = URL_BASE + FS(stopURL) + ID  + FS(stfQuery) + score[STF] + FS(bearsQuery)+score[BEARS];
	DEBUG_PRINTLN(url);
	if (transmit) {
		trySendData(url, 2, true);
	}
}

void trySendData(const String & url, int8_t numberOfRetries, boolean tryToReboot) {
	int8_t reInitCounter = numberOfRetries;
	int8_t warningCounter = (numberOfRetries*2);
	while (!sendData(url)) {
			if (reInitCounter-- <= 0 && tryToReboot) {
				reInitGPRS();
				break;
			}
			else {
				delay(500);
			}

			if (warningCounter-- <= 0) {
				// send sms????
				DEBUG_PRINTLN("W!!");
				break;
			}
	}

	digitalWrite(LED_BUILTIN, HIGH);
}

boolean sendData(const String & url) {
	delay(100);
	char c;
	uint16_t statuscode;
	uint16_t length;

	char urlToSend[115];
	url.toCharArray(urlToSend, 115);

	fona.flush();

	if (!fona.HTTP_GET_start(urlToSend, &statuscode, reinterpret_cast<uint16_t*>(&length))) {
		fona.flush();
		return false;
	}

	fona.flush();
	fona.HTTP_GET_end();
	fona.flush();

	return statuscode == 200;
}

