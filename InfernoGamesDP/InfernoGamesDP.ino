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
#include <MemoryFree.h>
#include "InfernoGamesDP.h"



#define FONA_RX 4
#define FONA_TX 5
#define FONA_RST 6

#define RED 0
#define BLUE 1
#define GREEN 2
#define YELLOW 3
#define NUMBER_OF_TEAMS 3

#define NO_TEAM 99
#define BEARS 0
#define STF 1
#define SOR 2
#define NA 3

#define LED_COLOR_RED pixels.Color(255, 0, 0)
#define LED_COLOR_BLUE pixels.Color(0, 0, 255)
#define LED_COLOR_GREEN pixels.Color(0, 255, 0)
#define LED_COLOR_YELLOW pixels.Color(255, 150, 0)
#define LED_COLOR_WHITE pixels.Color(255, 255, 255)
#define LED_COLOR_WHITE_LOW pixels.Color(25, 25, 25)

#define RED_BUTTON_PIN 2
#define BLUE_BUTTON_PIN 3
#define BUTTON_LED_PIN 9
#define STATUS_CAPTURING 1
#define STATUS_HOLDING 2
#define READY_TIME 3


#define ID 5
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
byte buttons[] = { A0, A1, A2 };
byte pressed[NUMBUTTONS], justpressed[NUMBUTTONS], justreleased[NUMBUTTONS];
byte previous_keystate[NUMBUTTONS], current_keystate[NUMBUTTONS];



void initFONA();
void trySendData(const String&, int8_t, boolean);
void setStatus(uint8_t, uint8_t);
void setAlive(boolean);
void reInitGPRS();
void checkResetState();
void reportStatusToWatchdog();
//void interpretResponse();
boolean sendData(const String&);

enum State {
	STANDBY,
	READY,
	NEUTRAL,
	TAKEN,
	END
};

struct TIME
{
	int8_t seconds;
	int8_t minutes;
	int8_t hours;
};
// this is a large buffer for replies


SoftwareSerial fonaSs = SoftwareSerial(FONA_TX, FONA_RX);
SoftwareSerial* fonaSerial = &fonaSs;
Adafruit_FONA fona = Adafruit_FONA(FONA_RST);
boolean fonaInitialized = false;

volatile State state;
byte currentTeam = NO_TEAM;
boolean standByModeIsSet = false;
boolean readyModeSet = false;
boolean neutralModeSet = false;
boolean goOnlineTimeIsSet = false;
volatile boolean resetState = false;
boolean resetReported = true;
char PIN[5] = "1234";
const String URL_BASE PROGMEM = "http://www.geeks.terminalprospect.com/AIR/";
uint16_t score[NUMBER_OF_TEAMS];
TIME startTime;
TIME goOnline;
TIME stopTime;
uint32_t loopCounter = 0;

#define FS(x) (__FlashStringHelper*)(x)
const char stf[]  PROGMEM = { "STF" };
const char bears[]  PROGMEM = { "BEARS" };
const char sor[]  PROGMEM = { "SoR" };

const char PROGMEM red[] = { "RED" };
const char PROGMEM blue[] = { "BLUE" };
const char PROGMEM green[] = { "GREEN" };
const char PROGMEM black[] = { "BLACK" };;

const char PROGMEM initializing[] = { "Initializing" };
const char PROGMEM simOk[] = { "SIM OK" };
const char PROGMEM gsmFound[] = { "GSM module found" };
const char PROGMEM networkFound[] = { "Network found" };

const char PROGMEM statusURL[] = { "UpdateStatus.php?ID=" };
const char PROGMEM teamQuery[] = { "&TEAM=" };
const char PROGMEM statusQuery[] = { "&STATUS=" };
const char PROGMEM watchdogURL[] = { "watchDog.php?ID=" };



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

uint16_t getTimeDiffInMinutes(struct TIME stop, struct TIME start) {
	return getTimeDiffInSeconds(stop, start) / 60;
}

uint16_t getTimeDiffInSeconds(struct TIME stop, struct TIME start) {
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

	return  ((hourDiff * 60 * 60) + (minDiff * 60) + secDiff);
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
		//Serial.println(pressed[index], DEC);
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

void writeTeamLogoToDisplay(byte team) {
	setGlobalTeamString(team);
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
	case SOR:
		globalTeamName = FS(sor);
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
	case NA:
		ledColor = LED_COLOR_YELLOW;
		break;
	case STF:
		ledColor = LED_COLOR_BLUE;
		break;
	case SOR:
		ledColor = LED_COLOR_GREEN;
		break;
	}
	pixels.fill(ledColor, 0, 8);
	pixels.show();
}


byte getTeamIdFromName(const String & team) {
	if (team.equalsIgnoreCase(FS(bears))) return BEARS;
	if (team.equalsIgnoreCase(FS(stf))) return STF;
	if (team.equalsIgnoreCase(FS(sor))) return SOR;
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
	case SOR:
		globalTeamColor = FS(green);
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


void handleButtons(byte pressedButton, byte currentTeam) {
	noTone(2);
	tone(2, 440, 300);

	if (pressedButton != currentTeam) {
		switch (pressedButton)
		{
		case RED:
			setTakenMode(BEARS);
			break;
		case YELLOW:
			setTakenMode(NA);
			break;
		case BLUE:
			setTakenMode(STF);
			break;
		case GREEN:
			setTakenMode(SOR);
			break;
		}
	}
}

boolean checkForSMS(char* smsbuff) {
	if (fonaInitialized) {
		char fonaInBuffer[64];
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
				if (fona.readSMS(slot, smsbuff, 40, &smslen)) {
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
		setStopTime(hours, (minutes + 1), seconds);
		goOnlineTimeIsSet = true;
	}
}

void setStopTime(const uint8_t & hours, const uint8_t & minutes, const uint8_t & seconds)
{
	stopTime.hours = hours;
	stopTime.minutes = minutes;
	stopTime.seconds = seconds;
}

void handleMessage(char* smsbuff) {
	String message = String(smsbuff);
	if (message.startsWith(F("STANDBY"))) {
		setStandbyMode(message.substring(8));
	}
	else if (message.startsWith(F("READY"))) {
		setReadyMode(message.substring(6));
	}
	else if (message.startsWith(F("NEUTRAL"))) {
		setNeutralMode();
	}
	else if (message.startsWith(F("TAKEN"))) {
		currentTeam = getTeamIdFromName(message.substring(6));
		setTakenMode(currentTeam);
	}
	else if (message.startsWith(F("STOP"))) {
		String time = message.substring(5);
		uint8_t hours = time.substring(0, 2).toInt();
		uint8_t minutes = time.substring(3, 5).toInt();
		uint8_t seconds = time.substring(6, 8).toInt();
		setStopTime(hours, minutes, seconds);
	}
}


void displayTransmittingText() {
	lcd.setFontSize(FONT_SIZE_SMALL);
	lcd.setCursor(0, 6);
	lcd.println(F("Transmitting data..."));
	lcd.print(F("Please stand by..."));
}

void removeTransmittingText() {
	lcd.setFontSize(FONT_SIZE_SMALL);
	lcd.setCursor(0, 6);
	lcd.println(F("                     "));
	lcd.print(F("                   "));
	printSignalLevelToDisplay();
}

void setStandbyMode(const String & onlineTime) {
	state = STANDBY;
	standByModeIsSet = true;
	setGoOnlineAndStopTime(onlineTime);
	delay(50);
	lcd.clear();
	pixels.clear();
	pixels.show();
	digitalWrite(BUTTON_LED_PIN, LOW);
	standByModeIsSet = true;
}

void setReadyMode(const String & onlineTime) {
	state = READY;
	setGoOnlineAndStopTime(onlineTime);
	delay(50);
	lcd.clear();
	pixels.clear();
	pixels.show();
	digitalWrite(BUTTON_LED_PIN, LOW);
}

void setNeutralMode() {
	state = NEUTRAL;
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
	TIME time = getTime();
	uint16_t timeCaptured = getTimeDiffInSeconds(time, startTime);

	if (currentTeam != NO_TEAM) {
		score[currentTeam] += timeCaptured;
	}
	else {
		setStartTime(getTime());
	}

	startTime = time;
	currentTeam = team;
	state = TAKEN;
	lcd.backlight(true);
	digitalWrite(BUTTON_LED_PIN, LOW);
	writeTeamLogoToDisplay(team);
	lightUpTeamColour(team);
	delay(50);
	displayTransmittingText();

	setStatus(team, 2);

	removeTransmittingText();
	digitalWrite(BUTTON_LED_PIN, HIGH);

}

void setEndMode() {
	TIME time = getTime();
	uint16_t timeCaptured = getTimeDiffInSeconds(time, startTime);

	if (currentTeam != NO_TEAM) {
		score[currentTeam] += timeCaptured;
	}

	digitalWrite(BUTTON_LED_PIN, LOW);
	setResult();
}

void setResult() {
	lcd.clear();

	byte maxIndex = 0;
	uint16_t maxValue = score[maxIndex];
	for (byte i = 1; i < NUMBER_OF_TEAMS; i++) {
		Serial.print(i);
		Serial.print(" ");
		Serial.println(score[i]);
		if (score[i] > maxValue) {
			maxValue = score[i];
			maxIndex = i;
		}
	}
	writeTeamLogoToDisplay(maxIndex);
	lightUpTeamColour(maxIndex);
	lcd.setCursor(0, 6);
	lcd.setFontSize(FONT_SIZE_SMALL);
	lcd.printInt(maxValue);
	lcd.print(F(" Points"));
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
	if ((n == 1) || (n == 2)) {
		lcd.setCursor(0, 0);
		lcd.draw(transmition[1], 16, 8);
	}
	if ((n >= 3) && (n <= 31)) {
		byte i = map(n, 2, 31, 2, 5);
		lcd.setCursor(0, 0);
		lcd.draw(transmition[i], 16, 8);
	}

}

void resetScore() {
	for (byte i = 1; i < NUMBER_OF_TEAMS; i++) {
		score[i] = 0;
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
	delay(100);
	initFONA(true);
	lcd.clear();
	lcd.setCursor(0, 0);
	printSignalLevelToDisplay();

	/*	Define Startup-State here: */
	setStandbyMode("");
	/********************************/
}

/**********************************************************************************
****************************    MAIN PROGRAM    ***********************************
************************************************************************************/
void loop() {
	Serial.print(F("freeMemory()="));
	Serial.println(freeMemory());

	while (1) {
		noTone(2);
		if (checkForSMS(messageContent)) {
			handleMessage(messageContent);
		}
		uint16_t timeLeft = 0;
		switch (state) {
		case STANDBY:
			if (!standByModeIsSet) {
				setStandbyMode("");
			}
			//if (loopCounter++ > 200000) {
			printSignalLevelToDisplay();
			if (goOnlineTimeIsSet && getTimeDiffInMinutes(goOnline, getTime()) < READY_TIME) {
				state = READY;
			}
			delay(5000);
			//loopCounter = 0;
		//}
			break;
		case READY:
			timeLeft = getTimeDiffInMinutes(goOnline, getTime());
			if (!readyModeSet && timeLeft <= READY_TIME) {
				setStatus(NO_TEAM, 1);
				readyModeSet = true;
			}
			if (timeLeft < 1) {
				setNeutralMode();
				break;
			}
			lcd.clear();
			lcd.setCursor(0, 4);
			lcd.setFontSize(FONT_SIZE_SMALL);
			lcd.print(F("Online in "));
			lcd.printInt(timeLeft);
			lcd.print(F(" minutes"));
			printSignalLevelToDisplay();
			delay(10000);
			break;
		case NEUTRAL:
			handleButtons(getPressedButton(), NO_TEAM);
			if (++loopCounter > 50000) {
				if (getTimeDiffInMinutes(stopTime, getTime()) < 1)
					state = END;
				loopCounter = 0;
			}
			break;
		case TAKEN:
			handleButtons(getPressedButton(), currentTeam);
			if (++loopCounter > 50000) {
				if (getTimeDiffInMinutes(stopTime, getTime()) < 1)
					state = END;
				loopCounter = 0;
			}
			break;
		case END:
			setEndMode();
			resetScore();
			while (1) {
				delay(5000);
				Serial.println(F("X"));
				if (checkForSMS(messageContent)) {
					handleMessage(messageContent);
					Serial.println(F("Y"));
					break;
				}
			}
			break;
		}
	}
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

void checkResetState() {
	if (resetState) {
		state = NEUTRAL;
		delay(20);
		resetReported = false;
		resetState = false;
	}
}

void reportStatusToWatchdog() {

	checkResetState();
	if (state == NEUTRAL && !resetReported) {
		setStatus(NO_TEAM, 2);
		resetReported = true;
	}
	else {
		setAlive(true);
	}
}

//void interpretResponse()
//{
//	String buffer(replybuffer);
//	if (buffer.startsWith(String('#'))) {
//		uint8_t teamEnd = buffer.indexOf('*');
//		uint8_t statusStart = teamEnd + 1;
//		String team = buffer.substring(1, teamEnd);
//		uint8_t status = buffer.substring(statusStart, statusStart + 1).toInt();
//
//		switch (status) {
//		case 2:
//			if (team.equals(F("BLUE"))) {
//				state = TAKEN;
//			}
//			else if (team.equals(F("RED"))) {
//				state = TAKEN;
//			}
//			else {
//				state = NEUTRAL;
//			}
//			break;
//		}
//		memset(replybuffer, 0, sizeof(replybuffer));
//	}
//}

void flushFONA() {
	flushSerial();
	while (fona.available()) {
		Serial.write(fona.read());
	}
	fona.flush();
}

void initFONA(boolean startup) {
	lcd.clear();
	lcd.setCursor(0, 0);
	if (startup) { writeStatusTextToDisplay(FS(initializing)); }

	fonaSerial->begin(4800);
	if (!fona.begin(*fonaSerial)) {
		while (true);
	}
	if (startup) { writeStatusTextToDisplay(FS(gsmFound)); }

	flushFONA();
	// Optionally configure a GPRS APN, username, and password.
	fona.setGPRSNetworkSettings(F(APN));


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

		delay(200);
	}
	flushFONA();
	fonaInitialized = true;

	if (startup) {

		for (int i = 0; i < 3; i++) {
			fona.deleteSMS(i);
		}
	}

	//TESTING RTC! WORKS!"
	fona.enableRTC(1);
	fona.enableNetworkTimeSync(true);
	// END RTC

	if (startup) { writeStatusTextToDisplay(FS(gsmFound)); }

	flushFONA();
}

void reInitGPRS() {
	fona.enableGPRS(false);
	delay(400);
	fona.enableGPRS(true);
	delay(1000);
}

void setStatus(uint8_t teamId, uint8_t status) {
	setCurrentTeamColor(teamId);
	const String url = URL_BASE + FS(statusURL) + ID + FS(teamQuery) + globalTeamColor + FS(statusQuery) + status;
	Serial.println(url);
	trySendData(url, 5, true);
}

void setAlive(boolean tryToReboot) {
	const String url = URL_BASE + FS(watchdogURL) + ID;
	trySendData(url, 2, tryToReboot);
	//interpretResponse();
}

void trySendData(const String & url, int8_t numberOfRetries, boolean tryToReboot) {
	digitalWrite(LED_BUILTIN, LOW);
	//fona.enableGPRS(true);
	delay(200);

	int8_t reInitCounter = numberOfRetries;
	while (!sendData(url)) {
		reInitGPRS();
		if (--reInitCounter >= 0 && tryToReboot) {
			initFONA(false);
			delay(1000);
			//reInitCounter = numberOfRetries;
		}
		else {
			break;
		}
	}

	digitalWrite(LED_BUILTIN, HIGH);
	//fona.enableGPRS(false);
}


boolean sendData(const String & url) {
	char replybuffer[255];
	uint16_t statuscode;
	int16_t length;
	uint8_t i = 0;

	char urlToSend[90];
	url.toCharArray(urlToSend, 90);
	delay(20);

	fona.flush();

	if (!fona.HTTP_GET_start(urlToSend, &statuscode, reinterpret_cast<uint16_t*>(&length))) {
		//Serial.println(F("Failed! sendding"));
		fona.flush();
		return false;
	}

	while (length > 0) {
		while (fona.available()) {
			replybuffer[i++] = fona.read();

			// Serial.write is too slow, we'll write directly to Serial register!
#if defined(__AVR_ATmega328P__) || defined(__AVR_ATmega168__)
			loop_until_bit_is_set(UCSR0A, UDRE0); /* Wait until data register empty. */
			UDR0 = replybuffer[i - 1];
#else
			Serial.write(c);
#endif
			length--;
			if (!length) break;
		}
	}

	fona.HTTP_GET_end();

	fona.flush();

	return statuscode == 200;

}

