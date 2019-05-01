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


#define FONA_RX 4
#define FONA_TX 5
#define FONA_RST 6

#define RED 0
#define BLUE 1
#define GREEN 2
#define YELLOW 3

#define BEARS 0
#define STF 1
#define SOT 2
#define NA 3

#define LED_COLOR_RED pixels.Color(255, 0, 0)
#define LED_COLOR_BLUE pixels.Color(0, 0, 255)
#define LED_COLOR_GREEN pixels.Color(0, 255, 0)
#define LED_COLOR_YELLOW pixels.Color(255, 150, 0)
#define LED_COLOR_WHITE pixels.Color(255, 255, 255)
#define LED_COLOR_WHITE_LOW pixels.Color(25, 25, 25)



#define NO_TEAM "BLACK"
#define CAPTURE_TIMEOUT 10
#define CONFIRM_TIMEOUT 10
#define RED_BUTTON_PIN 2
#define BLUE_BUTTON_PIN 3
#define BUTTON_LED_PIN 9
#define STATUS_CAPTURING 1
#define STATUS_HOLDING 2


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
void setStatus(const String&, uint8_t);
void setAlive(boolean);
void reInitGPRS();
void blueButtonISR();
void redButtonISR();
void checkResetState();
void reportStatusToWatchdog();
void interpretResponse();
boolean sendData(const String&);

enum State {
	STANDBY,
	READY,
	NEUTRAL,
	TAKEN
};
// this is a large buffer for replies
char replybuffer[255];

SoftwareSerial fonaSs = SoftwareSerial(FONA_TX, FONA_RX);
SoftwareSerial *fonaSerial = &fonaSs;
Adafruit_FONA fona = Adafruit_FONA(FONA_RST);
boolean fonaInitialized = false;

volatile State state;
String currentTeam = NO_TEAM;
boolean standByModeIsSet = false;
boolean statusHasChanged = false;
volatile boolean resetState = false;
boolean resetReported = true;
char PIN[5] = "1234";
const char URL_BASE[43] = "http://www.geeks.terminalprospect.com/AIR/";
long timer = 0;
long loopTimer = 0;


void setupDisplay() {
	lcd.begin();
	lcd.clear();
	lcd.setCursor(30, 3);
	lcd.setFontSize(FONT_SIZE_XLARGE);
	lcd.print("STARTED");
	delay(1000);
}

void setupNeoPixelBar() {
	pixels.begin();
	pixels.clear();
	pixels.fill(LED_COLOR_WHITE_LOW, 0, 8);
	pixels.show();
}

void setupButtons() {
	for (byte i = 0; i < NUMBUTTONS; i++) {
		pinMode(buttons[i], INPUT);
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
	Serial.println("Writing team logo: " + team);
	String displayText;
	switch (team)
	{
	case BEARS:
		displayText = F("BEARS");
		break;
	case NA:
		displayText = F("N/A");
		break;
	case STF:
		displayText = F("STF");
		break;
	case SOT:
		displayText = F("SOT");
		break;
	}
	lcd.clear();
	lcd.setFontSize(FONT_SIZE_XLARGE);
	lcd.setCursor((128 - (displayText.length() * 9)) / 2, 3);
	lcd.println(displayText);
}

void lightUpTeamColour(byte team) {
	Serial.println("Seting Team Color: " + team);
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
	case SOT:
		ledColor = LED_COLOR_GREEN;
		break;
	}
	pixels.fill(ledColor, 0, 8);
	pixels.show();
}


byte getTeamIdFromName(String team) {
	if (team.equalsIgnoreCase(F("BEARS"))) return BEARS;
	if (team.equalsIgnoreCase(F("NONE"))) return NA;
	if (team.equalsIgnoreCase(F("STF"))) return STF;
	if (team.equalsIgnoreCase(F("SOT"))) return SOT;
}

String getTeamColorFromId(byte team) {
	switch (team)
	{
	case BEARS:
		return F("RED");
		break;
	case NA:
		return F("YELLOW");
		break;
	case STF:
		return F("BLUE");
		break;
	case SOT:
		return F("GREEN");
		break;
	}
}

void writeStatusTextToDisplay(String displayText) {
	lcd.setFontSize(FONT_SIZE_SMALL);
	lcd.println(displayText);
}


void handleButtons(byte pressedButton) {
	noTone(8);
	tone(8, 523, 100);
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
		setTakenMode(SOT);
		break;
	}

}

boolean checkForSMS(char *smsbuff) {
	if (fonaInitialized) {
		char fonaInBuffer[64];
		char* bufPtr = fonaInBuffer;    //handy buffer pointer

		if (fona.available())      //any data available from the FONA?
		{
			int slot = 0;            //this will be the slot number of the SMS
			int charCount = 0;
			//Read the notification into fonaInBuffer
			do {
				*bufPtr = fona.read();
				Serial.write(*bufPtr);
				delay(1);
			} while ((*bufPtr++ != '\n') && (fona.available()) && (++charCount < (sizeof(fonaInBuffer) - 1)));

			//Add a terminal NULL to the notification string
			*bufPtr = 0;

			//Scan the notification string for an SMS received notification.
			//  If it's an SMS message, we'll get the slot number in 'slot'
			if (1 == sscanf(fonaInBuffer, "+CMTI: \"SM\",%d", &slot)) {
				uint16_t smslen;
				if (!fona.readSMS(slot, smsbuff, 31, &smslen)) {
				}
				if (fona.deleteSMS(slot)) {

				}
				return true;
			}
		}
	}
	return false;
}

void handleMessage(char *smsbuff) {
	String message = String(smsbuff);

	if (message.startsWith(F("STANDBY"))) {
		setStandbyMode();
	}
	else if (message.startsWith(F("READY"))) {
		state = READY;
	}
	else if (message.startsWith(F("NEUTRAL"))) {
		reportStatusToWatchdog();
		setNeutralMode();
	}
	else if (message.startsWith(F("TAKEN"))) {
		currentTeam = message.substring(6);
		byte team = getTeamIdFromName(currentTeam);

		setTakenMode(team);
	}
}

void displayTransmittingText() {
	lcd.setFontSize(FONT_SIZE_SMALL);
	lcd.setCursor(0, 6);
	lcd.println("Sending Data");
	lcd.print("Please stand by...");
}

void removeTransmittingText() {
	lcd.setFontSize(FONT_SIZE_SMALL);
	lcd.setCursor(0,6);
	lcd.println("                    ");
	lcd.print("                     ");
}

void setStandbyMode() {
	state = STANDBY;
	lcd.backlight(false);
	lcd.clear();
	pixels.clear();
	pixels.show();
	digitalWrite(BUTTON_LED_PIN, LOW);
	standByModeIsSet = true;
}

void setNeutralMode() {
	state = NEUTRAL;
	lcd.backlight(true);
	digitalWrite(BUTTON_LED_PIN, HIGH);
	pixels.fill(LED_COLOR_WHITE_LOW, 0, 8);
	pixels.show();
}

void setTakenMode(byte team) {
	state = TAKEN;
	lcd.backlight(true);
	digitalWrite(BUTTON_LED_PIN, LOW);
	writeTeamLogoToDisplay(team);
	lightUpTeamColour(team);
	delay(50);
	displayTransmittingText();
	delay(500);
	setStatus(getTeamColorFromId(team));
	delay(500);
	removeTransmittingText();
	digitalWrite(BUTTON_LED_PIN, HIGH);
}



/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */


/***********************************************************************************
*********************************    SETUP    **************************************
************************************************************************************/
void setup() {
	while (!Serial);
	Serial.begin(115200);
	delay(100);
	setupDisplay();
	setupNeoPixelBar();
	setupButtons();
	digitalWrite(BUTTON_LED_PIN, HIGH);
	state = NEUTRAL;
	delay(100);
	initFONA();
}

/**********************************************************************************
****************************    MAIN PROGRAM    ***********************************
************************************************************************************/
void loop() {
	char messageContent[10];
	Serial.print(F("freeMemory()="));
	Serial.println(freeMemory());
	lcd.clear();
	writeStatusTextToDisplay(F("LOOPING"));
	while (1) {
		if (checkForSMS(messageContent)) {
			handleMessage(messageContent);
		}

		switch (state) {
		case STANDBY:
			if (!standByModeIsSet) { setStandbyMode(); }
			break;
		case READY:
			break;
		case NEUTRAL:
			handleButtons(getPressedButton());
			break;
		case TAKEN:
			handleButtons(getPressedButton());
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
		setStatus(NO_TEAM);
		resetReported = true;
	}
	else {
		setAlive(true);
	}
	timer = 0;
	loopTimer = millis();
}

void interpretResponse()
{
	String buffer(replybuffer);
	if (buffer.startsWith(String('#'))) {
		uint8_t teamEnd = buffer.indexOf('*');
		uint8_t statusStart = teamEnd + 1;
		String team = buffer.substring(1, teamEnd);
		uint8_t status = buffer.substring(statusStart, statusStart + 1).toInt();

		switch (status) {
		case 2:
			if (team.equals(F("BLUE"))) {
				state = TAKEN;
			}
			else if (team.equals(F("RED"))) {
				state = TAKEN;
			}
			else {
				state = NEUTRAL;
			}
			break;
		}
		memset(replybuffer, 0, sizeof(replybuffer));
	}
}

void flushFONA() {
	flushSerial();
	while (fona.available()) {
		Serial.write(fona.read());
	}
	fona.flush();
}

void initFONA() {
	lcd.clear();
	lcd.setCursor(0, 0);
	writeStatusTextToDisplay(F("Initializing"));

	fonaSerial->begin(4800);
	if (!fona.begin(*fonaSerial)) {
		while (true);
	}
	writeStatusTextToDisplay(F("GSM Module Found"));

	// Optionally configure a GPRS APN, username, and password.
	fona.setGPRSNetworkSettings(F(APN));


	flushFONA();
	while (!fona.unlockSIM(PIN)) {
		delay(100);

		if (fona.getNetworkStatus() == 1)
			break;
	}
	writeStatusTextToDisplay(F("SIM OK"));

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

	writeStatusTextToDisplay(F("Network Found"));
	fona.enableGPRS(false);
	delay(100);
	while (!fona.enableGPRS(true)) {

		delay(200);
	}
	flushFONA();
	fonaInitialized = true;

	//TESTING RTC! WORKS!"
	fona.enableRTC(1);
	if (!fona.enableNetworkTimeSync(true)) {
		Serial.println(F("Failed to enable"));
	}
	char buffer[23];
	fona.getTime(buffer, 23);
	writeStatusTextToDisplay(buffer);
	Serial.println("Time: " + String(buffer));
	// END RTC
	writeStatusTextToDisplay(F("GSM init Finished"));

}

void reInitGPRS() {
	fona.enableGPRS(false);
	delay(100);
	fona.enableGPRS(true);
	delay(200);
}

void setStatus(const String& team) {
	const String url = String(URL_BASE) + F("UpdateStatus.php?ID=") + ID + F("&TEAM=") + team + F("&STATUS=2");
	trySendData(url, 5, true);
}

void setAlive(boolean tryToReboot) {
	const String url = String(URL_BASE) + F("watchDog.php?ID=") + ID;
	//const String url = "http://www.geeks.terminalprospect.com/AIR/watchDog.php?ID=5";
	Serial.println(url);
	trySendData(url, 2, tryToReboot);
	interpretResponse();
}

void trySendData(const String& url, int8_t numberOfRetries, boolean tryToReboot) {
	fona.enableGPRS(true);
	delay(200);

	int8_t reInitCounter = numberOfRetries;
	while (!sendData(url)) {
		reInitGPRS();
		if (--reInitCounter <= 0 && tryToReboot) {
			initFONA();
			delay(1000);
			reInitCounter = numberOfRetries;
		}
		else { break; }
	}

	digitalWrite(LED_BUILTIN, HIGH);
	fona.enableGPRS(false);
}


boolean sendData(const String& url) {
	uint16_t statuscode;
	int16_t length;
	uint8_t i = 0;

	char urlToSend[90];
	url.toCharArray(urlToSend, 90);
	delay(20);

	fona.flush();

	if (!fona.HTTP_GET_start(urlToSend, &statuscode, reinterpret_cast<uint16_t *>(&length))) {
		//Serial.println("Failed! sendding");
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
	//Serial.println(F("\n****"));
	fona.HTTP_GET_end();

	fona.flush();
	statusHasChanged = false;

	return true;

}

