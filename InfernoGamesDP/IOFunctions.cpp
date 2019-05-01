// 
// 
// 

#include "IOFunctions.h"


void flushSerial() {
	while (Serial.available()) {
		Serial.read();
	}

}