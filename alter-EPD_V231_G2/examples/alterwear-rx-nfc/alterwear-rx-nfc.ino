// -*- mode: c++ -*-
// Copyright 2013-2015 Pervasive Displays, Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at:
//
//   http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing,
// software distributed under the License is distributed on an
// "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either
// express or implied.  See the License for the specific language
// governing permissions and limitations under the License.

// Updated 2015-08-01 by Rei Vilo
// . Added #include Energia
// . For Energia, changed pin names to pin numbers (see comment below)
// . Works on MSP430F5529, LM4F120, TM4C123
// . Fails on MSP432 and CC3200

// Notice: ***** Generated file: DO _NOT_ MODIFY, Created on: 2016-01-12 00:11:21 UTC *****


// Simple demo to toggle EPD between two images.

// Operation from reset:
// * display version
// * display compiled-in display setting
// * display EPD FLASH detected or not
// * display temperature (displayed before every image is changed)
// * clear screen
// * delay 5 seconds (flash LED)
// * display text image
// * delay 5 seconds (flash LED)
// * display picture
// * delay 5 seconds (flash LED)
// * back to text display


#if defined(ENERGIA)
#include <Energia.h>
#else
#include <Arduino.h>
#include <EEPROM.h>
//i2c library
#include <Wire.h>
#endif

#include <inttypes.h>
#include <ctype.h>

// required libraries
#include <SPI.h>
#include <alter-EPD_FLASH.h>
#include <alter-EPD_V231_G2.h>
#define SCREEN_SIZE 200
#include <alter-EPD_PANELS.h>
#include <alter-S5813A.h>
#include <alter-EPD_PINOUT.h>

// select two images from:  text_image text-hello cat aphrodite venus saturn
#define IMAGE_1  text_image
#define IMAGE_2  cat

// Error message for MSP430
#if (SCREEN_SIZE == 270) && defined(__MSP430G2553__)
#error MSP430: not enough memory
#endif

// no futher changed below this point

// current version number
#define DEMO_VERSION "alterwear-rx-nfc"


// pre-processor convert to string
#define MAKE_STRING1(X) #X
#define MAKE_STRING(X) MAKE_STRING1(X)

// other pre-processor magic
// token joining and computing the string for #include
#define ID(X) X
#define MAKE_NAME1(X,Y) ID(X##Y)
#define MAKE_NAME(X,Y) MAKE_NAME1(X,Y)
#define MAKE_JOIN(X,Y) MAKE_STRING(MAKE_NAME(X,Y))

// calculate the include name and variable names
#define IMAGE_1_FILE MAKE_JOIN(IMAGE_1,EPD_IMAGE_FILE_SUFFIX)
#define IMAGE_1_BITS MAKE_NAME(IMAGE_1,EPD_IMAGE_NAME_SUFFIX)
#define IMAGE_2_FILE MAKE_JOIN(IMAGE_2,EPD_IMAGE_FILE_SUFFIX)
#define IMAGE_2_BITS MAKE_NAME(IMAGE_2,EPD_IMAGE_NAME_SUFFIX)


// Add Images library to compiler path
#include <Images.h>  // this is just an empty file

// images
PROGMEM const
#define unsigned
#define char uint8_t
#include IMAGE_1_FILE
#undef char
#undef unsigned

PROGMEM const
#define unsigned
#define char uint8_t
#include IMAGE_2_FILE
#undef char
#undef unsigned


// LED anode through resistor to I/O pin
// LED cathode to Ground
#define LED_ON  HIGH
#define LED_OFF LOW

//EEPROM
int eeprom_addr = 0;
int current_state = 13;

//i2c
#define SLAVE_ADDR 0x55
#define int_reg_addr 0x01      //first block of user memory
byte rxDataOne[17];
byte rxDataTwo[17];
byte converted[17];

// define the E-Ink display
EPD_Class EPD(EPD_SIZE,
	      Pin_PANEL_ON,
	      Pin_BORDER,
	      Pin_DISCHARGE,
#if EPD_PWM_REQUIRED
	      Pin_PWM,
#endif
	      Pin_RESET,
	      Pin_BUSY,
	      Pin_EPD_CS);

void setupPins(){
	pinMode(Pin_RED_LED, OUTPUT);
	pinMode(Pin_SW2, INPUT);
	pinMode(Pin_TEMPERATURE, INPUT);
#if EPD_PWM_REQUIRED
	pinMode(Pin_PWM, OUTPUT);
#endif
	pinMode(Pin_BUSY, INPUT);
	pinMode(Pin_RESET, OUTPUT);
	pinMode(Pin_PANEL_ON, OUTPUT);
	pinMode(Pin_DISCHARGE, OUTPUT);
	pinMode(Pin_BORDER, OUTPUT);
	pinMode(Pin_EPD_CS, OUTPUT);
	pinMode(Pin_EPD_FLASH_CS, OUTPUT);
}

void initializePins(){
	digitalWrite(Pin_RED_LED, LOW);
#if EPD_PWM_REQUIRED
	digitalWrite(Pin_PWM, LOW);
#endif
	digitalWrite(Pin_RESET, LOW);
	digitalWrite(Pin_PANEL_ON, LOW);
	digitalWrite(Pin_DISCHARGE, LOW);
	digitalWrite(Pin_BORDER, LOW);
	digitalWrite(Pin_EPD_CS, LOW);
	digitalWrite(Pin_EPD_FLASH_CS, HIGH);
}

void initializeSerial() {
	Serial.begin(9600);
	delay(500);

#if defined(__AVR__)
	// // indefinite wait for USB CDC serial port to connect.  Arduino Leonardo only
	// while (!Serial) {
	// }
	// additional delay for USB CDC serial port to connect.  Arduino Leonardo only
	if (!Serial) {       // allows terminal time to sync as long
		delay(500);  // as the serial monitor is opened before
	}                    // upload
#endif
}

void printEPDInfo(){
	Serial.println();
	Serial.println();
	Serial.println("Demo version: " DEMO_VERSION);
	Serial.println("Display size: " MAKE_STRING(EPD_SIZE));
	Serial.println("Film: V" MAKE_STRING(EPD_FILM_VERSION));
	Serial.println("COG: G" MAKE_STRING(EPD_CHIP_VERSION));
	Serial.println();

	Serial.println("Image 1: " IMAGE_1_FILE);
	Serial.println("Image 2: " IMAGE_2_FILE);
	Serial.println();
}

void initializeEPD(){
	EPD_FLASH.begin(Pin_EPD_FLASH_CS);
	if (EPD_FLASH.available()) {
		Serial.println("EPD FLASH chip detected OK");
	} else {
		uint8_t maufacturer;
		uint16_t device;
		EPD_FLASH.info(&maufacturer, &device);
		Serial.print("unsupported EPD FLASH chip: MFG: 0x");
		Serial.print(maufacturer, HEX);
		Serial.print("  device: 0x");
		Serial.print(device, HEX);
		Serial.println();
	}
	// configure temperature sensor
	S5813A.begin(Pin_TEMPERATURE);
}


// from comment #4: https://forum.arduino.cc/index.php?topic=241663.0
unsigned long convertFromHex(int ascii){ 
 if(ascii > 0x39) ascii -= 7; // adjust for hex letters upper or lower case
 return(ascii & 0xf);
}

// I/O setup
void setup() {
	setupPins();
	initializePins();
	initializeSerial();
	Wire.begin();                         // join i2c bus 
	printEPDInfo();
	initializeEPD();	

/*
	float start = millis();
	int x = 300;
	for (uint8_t addr = 0; addr < x; addr++) {
		EEPROM.write(addr, current_state+addr);
	}
	float stop = millis();
	Serial.print("time to write ");
	Serial.print(x);
	Serial.print(" bytes: ");
	Serial.println(stop-start);
	*/
	
	/*
	Serial.println("trying to see the image bits....");

	Serial.print("image_1_bits DEC: ");
	Serial.println((byte)IMAGE_1_BITS);
	Serial.print("image_1_bits HEX: ");
	Serial.println((byte)IMAGE_1_BITS, HEX);
	Serial.print("image_1_bits BIN: ");
	Serial.println((byte)IMAGE_1_BITS, BIN);
	Serial.print("image_1_file: ");
	Serial.println(IMAGE_1_FILE);              // idea: try this from the EPD lib code.
	*/

/*
	uint8_t pixels;
	// this->bytes_per_line is 200/8 = 25 for 2_0
	for (uint16_t b = 0; b < 25; ++b) {
		//pixels = pgm_read_byte_near(IMAGE_1_BITS + b) & 0xaa;
		pixels = pgm_read_byte_near(IMAGE_1_BITS + b);
		Serial.print("pixels: ");
		Serial.println(pixels);
	}
	for (int i = 0; i < 100; i++) {
		Serial.print("0x");
		Serial.print(IMAGE_1_BITS[i], HEX);
		Serial.print(", ");
		if (i%12 == 0) {
			Serial.println();
		}
	}
	*/


	Serial.println("set up complete");
}

int getTemperature() {
	int temperature = S5813A.read();
	Serial.print("Temperature = ");
	Serial.print(temperature, DEC);
	Serial.println(" Celsius");
	return temperature;
}

void checkEPD() {
	EPD.begin(); // power up the EPD panel
	switch (EPD.error()) {
	case EPD_OK:
		Serial.println("EPD: ok");
		break;
	case EPD_UNSUPPORTED_COG:
		Serial.println("EPD: unsuppported COG");
		break;
	case EPD_PANEL_BROKEN:
		Serial.println("EPD: panel broken");
		break;
	case EPD_DC_FAILED:
		Serial.println("EPD: DC failed");
		break;
	}
}

void flashLED(int delay_count) {
	// flash LED for 5 seconds
	for (int x = 0; x < delay_count; ++x) {
		digitalWrite(Pin_RED_LED, LED_ON);
		delay(50);
		digitalWrite(Pin_RED_LED, LED_OFF);
		delay(50);
	}
}

void initializeNFCTransmission(){
	Wire.beginTransmission(byte(SLAVE_ADDR));
  	Wire.write(int_reg_addr);
  	Wire.endTransmission();
  	Wire.requestFrom(byte(SLAVE_ADDR),16);
}

void readFromNFC(){
	Serial.println(Wire.available());
	if (Wire.available()){
		Serial.println("Wire Available, reading now...");

		// you always have to read data in chunks of 16 or it'll fail, 
		// the first 9 bytes are ntag metadata,
		for (int i = 0; i < 16; i++) {
			//rxDataOne[i] = Wire.read();
			//EEPROM.write(eeprom_addr, current_state);
			byte temp = Wire.read();
			if (i > 8){
				Serial.print(temp); // data received
				Serial.print(" , ");
				temp = convertFromHex(temp); // convert data to correct value representation
				Serial.println(convertFromHex(temp));
				EEPROM.write(eeprom_addr+i, temp);
				// rxDataOne[i] = temp;
			}
		}
	}
}

void convert() {
	for (int i = 0; i < 16; i++) {
		if (i > 8) {
			converted[i] = convertFromHex(rxDataOne[i]);
			Serial.print("rxDataOne i: ");
			Serial.print(i);
			Serial.print(", byte: ");
			Serial.print(rxDataOne[i]);
			Serial.print(", converted: ");
			long converted = convertFromHex(rxDataOne[i]);
			Serial.println(converted);
		}
	}
}

void sanityCheckEeprom() {
	Serial.println("sanity checking eeprom contents...");
	for (int i = 0; i < 16; i++) {
		if (i > 8) {
			//rxDataOne[i] = Wire.read();
			//EEPROM.write(eeprom_addr, current_state);
			byte cetl = EEPROM.read(eeprom_addr+i);
			Serial.println(cetl);
		}
	}
}
// main loop
void loop() {

	initializeNFCTransmission();
	readFromNFC();

	// Arianna's Next Steps:
	// figure out how to transfer more than 16 bytes
	// maybe some kind of for loop where you do this over and over again: 
	// {
	// initializeNFCTransmission();
	// readFromNFC(address to read from next);
	// }

	sanityCheckEeprom();
	
	// eink logistics
	int temperature = getTemperature();
	checkEPD();
	EPD.setFactor(temperature); // adjust for current temperature
	EPD.clear(); // always clear screen at the beginning.
	flashLED(5); // reduce delay so first image comes up quickly

	// currently running
	float start = millis();
	Serial.print("Time to run ");
	EPD.image_eeprom(eeprom_addr);
	float stop = millis();
	Serial.print(", took: ");
	Serial.print(stop-start);
	Serial.print(" ms");
	// No good: Video #1, 4, 8 (interlace failed), 10 (smear)
	// Good, video: 2, 3, 5, 6 (lines), 7 (subset), 9 (half flip), 11 (smear, 2nd loop is better)

	//Time to run alter-epd-v231-g2, image_fast, took: 997.00 ms

	// offset:
	//EPD.image_eeprom(eeprom_addr);
	// not offset
	//EPD.image_eeprom(IMAGE_1_BITS);
	// not offset:
	//EPD.image_0(IMAGE_1_BITS);
	flashLED(50); // keep next image up for a bit.

	EPD.end();   // power down the EPD panel
	flashLED(100);
	/*

	for (uint16_t addr = 0; addr < IMAGE_1_BITS; addr++) {
		byte val = pgm_read_byte_near(addr);
		Serial.print("pgm: ");
		Serial.println(val);

		//current_state = EEPROM.read(addr);
		//Serial.print("current_state, arduino: ");
		//Serial.print(current_state);
		
	}	
	*/
}