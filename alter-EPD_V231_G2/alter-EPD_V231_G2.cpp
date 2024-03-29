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

#if defined(ENERGIA)
#include <Energia.h>
#else
#include <Arduino.h>
#endif

#include <limits.h>

#include <SPI.h>

#include "alter-EPD_V231_G2.h"

// delays - more consistent naming
#define Delay_ms(ms) delay(ms)
#define Delay_us(us) delayMicroseconds(us)

// inline arrays
#define ARRAY(type, ...) ((type[]){__VA_ARGS__})
#define CU8(...) (ARRAY(const uint8_t, __VA_ARGS__))

// values for border byte
#define BORDER_BYTE_BLACK 0xff
#define BORDER_BYTE_WHITE 0xaa
#define BORDER_BYTE_NULL  0x00

static void SPI_on(void);
static void SPI_off(void);
static void SPI_put(uint8_t c);
static void SPI_send(uint8_t cs_pin, const uint8_t *buffer, uint16_t length);
static uint8_t SPI_read(uint8_t cs_pin, const uint8_t *buffer, uint16_t length);


EPD_Class::EPD_Class(EPD_size _size,
		     uint8_t panel_on_pin,
		     uint8_t border_pin,
		     uint8_t discharge_pin,
		     uint8_t reset_pin,
		     uint8_t busy_pin,
		     uint8_t chip_select_pin) :
	EPD_Pin_PANEL_ON(panel_on_pin),
	EPD_Pin_BORDER(border_pin),
	EPD_Pin_DISCHARGE(discharge_pin),
	EPD_Pin_RESET(reset_pin),
	EPD_Pin_BUSY(busy_pin),
	EPD_Pin_EPD_CS(chip_select_pin),
	size(_size) {

	this->base_stage_time = 480; // milliseconds
	this->lines_per_display = 96;
	this->dots_per_line = 128;
	this->bytes_per_line = 128 / 8;
	this->bytes_per_scan = 96 / 4;

	this->middle_scan = true; // => data-scan-data ELSE: scan-data-scan
	this->pre_border_byte = false;
	this->border_byte = EPD_BORDER_BYTE_ZERO;

	// display size dependant items
	{
		static uint8_t cs[] = {0x72, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0f, 0xff, 0x00};
		this->channel_select = cs;
		this->channel_select_length = sizeof(cs);
	}

	// set up size structure
	switch (size) {
	default:
	case EPD_1_44:  // default so no change
		break;

	case EPD_1_9: {
		this->lines_per_display = 128;
		this->bytes_per_line = 144 / 8;
		this->middle_scan = false;
		this->bytes_per_scan = 128 / 4 / 2; // scan/2 - data - scan/2
		static uint8_t cs[] = {0x72, 0x00, 0x00, 0x00, 0x03, 0xfc, 0x00, 0x00, 0xff};
		this->channel_select = cs;
		this->channel_select_length = sizeof(cs);
		this->pre_border_byte = false;
		this->border_byte = EPD_BORDER_BYTE_SET;
		break;
	}

	case EPD_2_0: {
		this->lines_per_display = 96;
		this->dots_per_line = 200;
		this->bytes_per_line = 200 / 8;
		this->bytes_per_scan = 96 / 4;
		static uint8_t cs[] = {0x72, 0x00, 0x00, 0x00, 0x00, 0x01, 0xff, 0xe0, 0x00};
		this->channel_select = cs;
		this->channel_select_length = sizeof(cs);
		this->pre_border_byte = true;
		this->border_byte = EPD_BORDER_BYTE_NONE;
		break;
	}

	case EPD_2_6: {
		this->base_stage_time = 630; // milliseconds
		this->lines_per_display = 128;
		this->dots_per_line = 232;
		this->bytes_per_line = 232 / 8;
		this->middle_scan = false;
		this->bytes_per_scan = 128 / 4 / 2; // scan/2 - data - scan/2
		static uint8_t cs[] = {0x72, 0x00, 0x00, 0x1f, 0xe0, 0x00, 0x00, 0x00, 0xff};
		this->channel_select = cs;
		this->channel_select_length = sizeof(cs);
		this->pre_border_byte = false;
		this->border_byte = EPD_BORDER_BYTE_SET;
		break;
	}

	case EPD_2_7: {
		this->base_stage_time = 630; // milliseconds
		this->lines_per_display = 176;
		this->dots_per_line = 264;
		this->bytes_per_line = 264 / 8;
		this->bytes_per_scan = 176 / 4;
		static uint8_t cs[] = {0x72, 0x00, 0x00, 0x00, 0x7f, 0xff, 0xfe, 0x00, 0x00};
		this->channel_select = cs;
		this->channel_select_length = sizeof(cs);
		this->pre_border_byte = true;
		this->border_byte = EPD_BORDER_BYTE_NONE;
		break;
	}
	}

	this->factored_stage_time = this->base_stage_time; // milliseconds
	this->setFactor(); // ensure default temperature

}


void EPD_Class::begin(void) {

	// assume ok
	this->status = EPD_OK;

	// power up sequence
	digitalWrite(this->EPD_Pin_RESET, LOW);
	digitalWrite(this->EPD_Pin_PANEL_ON, LOW);
	digitalWrite(this->EPD_Pin_DISCHARGE, LOW);
	digitalWrite(this->EPD_Pin_BORDER, LOW);
	digitalWrite(this->EPD_Pin_EPD_CS, LOW);

	SPI_on();

	Delay_ms(5);
	digitalWrite(this->EPD_Pin_PANEL_ON, HIGH);
	Delay_ms(10);

	digitalWrite(this->EPD_Pin_RESET, HIGH);
	digitalWrite(this->EPD_Pin_BORDER, HIGH);
	digitalWrite(this->EPD_Pin_EPD_CS, HIGH);
	Delay_ms(5);

	digitalWrite(this->EPD_Pin_RESET, LOW);
	Delay_ms(5);

	digitalWrite(this->EPD_Pin_RESET, HIGH);
	Delay_ms(5);

	// wait for COG to become ready
	while (HIGH == digitalRead(this->EPD_Pin_BUSY)) {
		Delay_us(10);
	}

	// read the COG ID
	int cog_id = SPI_read(this->EPD_Pin_EPD_CS, CU8(0x71, 0x00), 2);
	cog_id = SPI_read(this->EPD_Pin_EPD_CS, CU8(0x71, 0x00), 2);

	if (0x02 != (0x0f & cog_id)) {
		this->status = EPD_UNSUPPORTED_COG;
		this->power_off();
		return;
	}

	// Disable OE
	SPI_send(this->EPD_Pin_EPD_CS, CU8(0x70, 0x02), 2);
	SPI_send(this->EPD_Pin_EPD_CS, CU8(0x72, 0x40), 2);

	// check breakage
	SPI_send(this->EPD_Pin_EPD_CS, CU8(0x70, 0x0f), 2);
	int broken_panel = SPI_read(this->EPD_Pin_EPD_CS, CU8(0x73, 0x00), 2);
	if (0x00 == (0x80 & broken_panel)) {
		this->status = EPD_PANEL_BROKEN;
		this->power_off();
		return;
	}

	// power saving mode
	SPI_send(this->EPD_Pin_EPD_CS, CU8(0x70, 0x0b), 2);
	SPI_send(this->EPD_Pin_EPD_CS, CU8(0x72, 0x02), 2);

	// channel select
	SPI_send(this->EPD_Pin_EPD_CS, CU8(0x70, 0x01), 2);
	SPI_send(this->EPD_Pin_EPD_CS, this->channel_select, this->channel_select_length);

	// high power mode osc
	SPI_send(this->EPD_Pin_EPD_CS, CU8(0x70, 0x07), 2);
	SPI_send(this->EPD_Pin_EPD_CS, CU8(0x72, 0xd1), 2);

	// power setting
	SPI_send(this->EPD_Pin_EPD_CS, CU8(0x70, 0x08), 2);
	SPI_send(this->EPD_Pin_EPD_CS, CU8(0x72, 0x02), 2);

	// Vcom level
	SPI_send(this->EPD_Pin_EPD_CS, CU8(0x70, 0x09), 2);
	SPI_send(this->EPD_Pin_EPD_CS, CU8(0x72, 0xc2), 2);

	// power setting
	SPI_send(this->EPD_Pin_EPD_CS, CU8(0x70, 0x04), 2);
	SPI_send(this->EPD_Pin_EPD_CS, CU8(0x72, 0x03), 2);

	// driver latch on
	SPI_send(this->EPD_Pin_EPD_CS, CU8(0x70, 0x03), 2);
	SPI_send(this->EPD_Pin_EPD_CS, CU8(0x72, 0x01), 2);

	// driver latch off
	SPI_send(this->EPD_Pin_EPD_CS, CU8(0x70, 0x03), 2);
	SPI_send(this->EPD_Pin_EPD_CS, CU8(0x72, 0x00), 2);

	Delay_ms(5);

	bool dc_ok = false;

	for (int i = 0; i < 4; ++i) {
		// charge pump positive voltage on - VGH/VDL on
		SPI_send(this->EPD_Pin_EPD_CS, CU8(0x70, 0x05), 2);
		SPI_send(this->EPD_Pin_EPD_CS, CU8(0x72, 0x01), 2);

		Delay_ms(240);

		// charge pump negative voltage on - VGL/VDL on
		SPI_send(this->EPD_Pin_EPD_CS, CU8(0x70, 0x05), 2);
		SPI_send(this->EPD_Pin_EPD_CS, CU8(0x72, 0x03), 2);

		Delay_ms(40);

		// charge pump Vcom on - Vcom driver on
		SPI_send(this->EPD_Pin_EPD_CS, CU8(0x70, 0x05), 2);
		SPI_send(this->EPD_Pin_EPD_CS, CU8(0x72, 0x0f), 2);

		Delay_ms(40);

		// check DC/DC
		SPI_send(this->EPD_Pin_EPD_CS, CU8(0x70, 0x0f), 2);
		int dc_state = SPI_read(this->EPD_Pin_EPD_CS, CU8(0x73, 0x00), 2);
		if (0x40 == (0x40 & dc_state)) {
			dc_ok = true;
			break;
		}
	}
	if (!dc_ok) {
		this->status = EPD_DC_FAILED;
		this->power_off();
		return;
	}
	// output enable to disable
	SPI_send(this->EPD_Pin_EPD_CS, CU8(0x70, 0x02), 2);
	SPI_send(this->EPD_Pin_EPD_CS, CU8(0x72, 0x40), 2);

	SPI_off();
}


void EPD_Class::end(void) {

	this->nothing_frame();

	if (EPD_2_7 == this->size) {
		this->dummy_line();
		// only pulse border pin for 2.70" EPD
		Delay_ms(25);
		digitalWrite(this->EPD_Pin_BORDER, LOW);
		Delay_ms(200);
		digitalWrite(this->EPD_Pin_BORDER, HIGH);
	} else {
		this->border_dummy_line();
		Delay_ms(200);
	}

	SPI_on();

	// ??? - not described in datasheet
	SPI_send(this->EPD_Pin_EPD_CS, CU8(0x70, 0x0b), 2);
	SPI_send(this->EPD_Pin_EPD_CS, CU8(0x72, 0x00), 2);

	// latch reset turn on
	SPI_send(this->EPD_Pin_EPD_CS, CU8(0x70, 0x03), 2);
	SPI_send(this->EPD_Pin_EPD_CS, CU8(0x72, 0x01), 2);

	// power off charge pump Vcom
	SPI_send(this->EPD_Pin_EPD_CS, CU8(0x70, 0x05), 2);
	SPI_send(this->EPD_Pin_EPD_CS, CU8(0x72, 0x03), 2);

	// power off charge pump neg voltage
	SPI_send(this->EPD_Pin_EPD_CS, CU8(0x70, 0x05), 2);
	SPI_send(this->EPD_Pin_EPD_CS, CU8(0x72, 0x01), 2);

	Delay_ms(120);

	// discharge internal
	SPI_send(this->EPD_Pin_EPD_CS, CU8(0x70, 0x04), 2);
	SPI_send(this->EPD_Pin_EPD_CS, CU8(0x72, 0x80), 2);

	// power off all charge pumps
	SPI_send(this->EPD_Pin_EPD_CS, CU8(0x70, 0x05), 2);
	SPI_send(this->EPD_Pin_EPD_CS, CU8(0x72, 0x00), 2);

	// turn of osc
	SPI_send(this->EPD_Pin_EPD_CS, CU8(0x70, 0x07), 2);
	SPI_send(this->EPD_Pin_EPD_CS, CU8(0x72, 0x01), 2);

	Delay_ms(50);

	this->power_off();
}

void EPD_Class::power_off(void) {

	// turn of power and all signals
	digitalWrite(this->EPD_Pin_RESET, LOW);
	digitalWrite(this->EPD_Pin_PANEL_ON, LOW);
	digitalWrite(this->EPD_Pin_BORDER, LOW);

	// ensure SPI MOSI and CLOCK are Low before CS Low
	SPI_off();
	digitalWrite(this->EPD_Pin_EPD_CS, LOW);

	// pulse discharge pin
	digitalWrite(this->EPD_Pin_DISCHARGE, HIGH);
	Delay_ms(150);
	digitalWrite(this->EPD_Pin_DISCHARGE, LOW);
}


// internal functions
// ==================


// convert a temperature in Celcius to
// the scale factor for frame_*_repeat methods
int EPD_Class::temperature_to_factor_10x(int temperature) const {
	if (temperature <= -10) {
		return 170;
	} else if (temperature <= -5) {
		return 120;
	} else if (temperature <= 5) {
		return 80;
	} else if (temperature <= 10) {
		return 40;
	} else if (temperature <= 15) {
		return 30;
	} else if (temperature <= 20) {
		return 20;
	} else if (temperature <= 40) {
		return 10;
	}
	return 7;
}


// One frame of data is the number of lines * rows. For example:
// The 1.44” frame of data is 96 lines * 128 dots.
// The 2” frame of data is 96 lines * 200 dots.
// The 2.7” frame of data is 176 lines * 264 dots.

// the image is arranged by line which matches the display size
// so smallest would have 96 * 32 bytes

void EPD_Class::frame_fixed(uint8_t fixed_value, EPD_stage stage) {
	// Can I skip "even" or "odd" lines here, to save time?
	for (uint8_t line = 0; line < this->lines_per_display ; ++line) {
		this->line(line, 0, fixed_value, false, stage);
	}
}

void EPD_Class::frame_data(PROGMEM const uint8_t *image, EPD_stage stage, ALTERWEAR_EFFECT effect, int *turn_on=nullptr, int size=0){
	int index = 0;
	switch(effect){
	case ALTERWEAR_SMEAR:
		if (size > 0) {
			for (uint8_t line = 0; line < this->lines_per_display ; ++line) {
				if ( (index < size) && (turn_on[index] > 0) && (turn_on[index] == line)) {
					this->line(line, &image[line * this->bytes_per_line], 0, true, stage);
					index++;
				} else {
					// don't turn on the line...
				}
			}
		}
		break;
	case ALTERWEAR_FLIP:
		for (uint8_t line = 0; line < this->lines_per_display ; ++line) {
			this->line(line, &image[line * this->bytes_per_line], 0, true, stage, effect);
		}
		break;
	case ALTERWEAR_HALF_FLIP:
		for (uint8_t line = 0; line < this->lines_per_display ; ++line) {
			this->line(line, &image[line * this->bytes_per_line], 0, true, stage, effect);
		}
		break;
	case ALTERWEAR_EVEN_ONLY:
		for (uint8_t line = 0; line < this->lines_per_display ; ++line) {
			this->line(line, &image[line * this->bytes_per_line], 0, true, stage, effect);
		}
	case ALTERWEAR_IDK:
		for (int index = 0; index < size; index++) {
			if (turn_on[index] > 0) {
				// line definition:
				//void EPD_Class::line(uint16_t line, const uint8_t *data, uint8_t fixed_value, bool read_progmem, EPD_stage stage, bool flip=false) {
				
				// passing in turn_on[index] results in a random series of bits flipped on.
				// They look like a sub-set of the image, but they aren't.
				// The lines extend across the long way of the 2.0 eink display.
				this->line(turn_on[index], &image[index * this->bytes_per_line], 0, true, stage);

				// passing line but using index as the index results in vertical stripes
				//this->line(line, &image[index * this->bytes_per_line], 0, true, stage, );
			}
		}
		break;
	case ALTERWEAR_SUBSET:
		for (int index = 0; index < size; ++index) {
			this->line(turn_on[index], &image[turn_on[index] * this->bytes_per_line], 0, true, stage);
		}
		break;
	case ALTERWEAR_VERTICAL_LINES:
		for (uint8_t line = 0; line < this->lines_per_display ; ++line) {
			if (turn_on[index] > 0) {
				// passing line but using index as the index results in vertical stripes
				this->line(line, &image[index * this->bytes_per_line], 0, true, stage);
			}
		}
		break;
	case ALTERWEAR_EEPROM: // same as DEFAULT, just pass the effect flag through
		for (uint8_t line = 0; line < this->lines_per_display ; ++line) {
			this->line(line, &image[line * this->bytes_per_line], 0, true, stage, effect);
		}
	case ALTERWEAR_DEFAULT:
		for (uint8_t line = 0; line < this->lines_per_display ; ++line) {
			this->line(line, &image[line * this->bytes_per_line], 0, true, stage);
		}
	}
}


#if defined(EPD_ENABLE_EXTRA_SRAM)
void EPD_Class::frame_sram(const uint8_t *image, EPD_stage stage){
	for (uint8_t line = 0; line < this->lines_per_display ; ++line) {
		this->line(line, &image[line * this->bytes_per_line], 0, false, stage);
	}
}
#endif


void EPD_Class::frame_cb(uint32_t address, EPD_reader *reader, EPD_stage stage) {
	static uint8_t buffer[264 / 8];
	for (uint8_t line = 0; line < this->lines_per_display ; ++line) {
		reader(buffer, address + line * this->bytes_per_line, this->bytes_per_line);
		this->line(line, buffer, 0, false, stage);
	}
}


// gets called from image() functions in .h file, more or less directly from Arduino
void EPD_Class::frame_fixed_repeat(uint8_t fixed_value, EPD_stage stage) {
	// 480 milliseconds for all board sizes except 2.6 and 2.7
	long stage_time = this->factored_stage_time;
	do {
		unsigned long t_start = millis();
		// this next code is the one that actually goes through line by line.
		this->frame_fixed(fixed_value, stage);
		unsigned long t_end = millis();
		if (t_end > t_start) {
			stage_time -= t_end - t_start;
		} else {
			stage_time -= t_start - t_end + 1 + ULONG_MAX;
		}
	} while (stage_time > 0); // calls frame_fixed again if there's more time?
}


void EPD_Class::frame_data_repeat(PROGMEM const uint8_t *image, EPD_stage stage, ALTERWEAR_EFFECT effect=ALTERWEAR_DEFAULT, int *turn_on=nullptr, int turn_on_size=0) {
	long stage_time = this->factored_stage_time;
	do {
		unsigned long t_start = millis();
		this->frame_data(image, stage, effect, turn_on, turn_on_size);
		unsigned long t_end = millis();
		if (t_end > t_start) {
			stage_time -= t_end - t_start;
		} else {
			stage_time -= t_start - t_end + 1 + ULONG_MAX;
		}
	} while (stage_time > 0);
}


#if defined(EPD_ENABLE_EXTRA_SRAM)
void EPD_Class::frame_sram_repeat(const uint8_t *image, EPD_stage stage) {
	long stage_time = this->factored_stage_time;
	do {
		unsigned long t_start = millis();
		this->frame_sram(image, stage);
		unsigned long t_end = millis();
		if (t_end > t_start) {
			stage_time -= t_end - t_start;
		} else {
			stage_time -= t_start - t_end + 1 + ULONG_MAX;
		}
	} while (stage_time > 0);
}
#endif


void EPD_Class::frame_cb_repeat(uint32_t address, EPD_reader *reader, EPD_stage stage) {
	long stage_time = this->factored_stage_time;
	do {
		unsigned long t_start = millis();
		this->frame_cb(address, reader, stage);
		unsigned long t_end = millis();
		if (t_end > t_start) {
			stage_time -= t_end - t_start;
		} else {
			stage_time -= t_start - t_end + 1 + ULONG_MAX;
		}
	} while (stage_time > 0);
}


// pixels on display are numbered from 1 so even is actually bits 1,3,5,...
void EPD_Class::even_pixels(const uint8_t *data, uint8_t fixed_value, bool read_progmem, EPD_stage stage, ALTERWEAR_EFFECT effect=ALTERWEAR_DEFAULT) {
	for (uint8_t b = 0; b < this->bytes_per_line; b++) { // bytes_per_line = 16
		if (0 != data) {
#if !defined(__AVR__)
			uint8_t pixels = data[b] & 0xaa;
#else
			// AVR has multiple memory spaces
			uint8_t pixels;

			if (effect==ALTERWEAR_EEPROM) {
				// & 0xaa to turn on only the even pixels to display 
				// pixels = EEPROM.read(data+b) & 0xaa;

				byte value = EEPROM.read(*data+b);

				pixels = EEPROM.read(*data+b) & 0xaa;
				// Serial.print("'data' val .cpp: ");
				Serial.print(*data+b);
				// Serial.print("eeprom val .cpp: ");
				Serial.print(" , ");
				Serial.println(value);
				read_progmem=false;
				delay(1000);
			}
			
			if (read_progmem) {
				pixels = pgm_read_byte_near(data + b) & 0xaa;
				//Serial.print("address: ");
				//Serial.print( char(data+b) );
				//Serial.print(", pixels: ");
				//Serial.println(pixels);
			} else {
				pixels = data[b] & 0xaa;
			}
#endif
			switch(stage) {
			case EPD_compensate:  // B -> W, W -> B (Current Image)
				pixels = 0xaa | ((pixels ^ 0xaa) >> 1);
				break;
			case EPD_white:       // B -> N, W -> W (Current Image)
				pixels = 0x55 + ((pixels ^ 0xaa) >> 1);
				break;
			case EPD_inverse:     // B -> N, W -> B (New Image)
				pixels = 0x55 | (pixels ^ 0xaa);
				break;
			case EPD_normal:       // B -> B, W -> W (New Image)
				pixels = 0xaa | (pixels >> 1);
				break;
			}
			uint8_t p1 = (pixels >> 6) & 0x03;
			uint8_t p2 = (pixels >> 4) & 0x03;
			uint8_t p3 = (pixels >> 2) & 0x03;
			uint8_t p4 = (pixels >> 0) & 0x03;
			pixels = (p1 << 0) | (p2 << 2) | (p3 << 4) | (p4 << 6);
			SPI_put(pixels);
		} else {
			SPI_put(fixed_value);
		}
	}
}

// pixels on display are numbered from 1 so odd is actually bits 0,2,4,...
void EPD_Class::odd_pixels(const uint8_t *data, uint8_t fixed_value, bool read_progmem, EPD_stage stage) {
	for (uint16_t b = this->bytes_per_line; b > 0; --b) {
		if (0 != data) {
#if !defined(__AVR__)
			uint8_t pixels = data[b - 1] & 0x55;
#else
			// AVR has multiple memory spaces
			uint8_t pixels;
			if (read_progmem) {
				pixels = pgm_read_byte_near(data + b- 1) & 0x55;
			} else {
				pixels = data[b - 1] & 0x55;
			}
#endif
			switch(stage) {
			case EPD_compensate:  // B -> W, W -> B (Current Image)
				pixels = 0xaa | (pixels ^ 0x55);
				break;
			case EPD_white:       // B -> N, W -> W (Current Image)
				pixels = 0x55 + (pixels ^ 0x55);
				break;
			case EPD_inverse:     // B -> N, W -> B (New Image)
				pixels = 0x55 | ((pixels ^ 0x55) << 1);
				break;
			case EPD_normal:       // B -> B, W -> W (New Image)
				pixels = 0xaa | pixels;
				break;
			}
			SPI_put(pixels);
		} else {
			SPI_put(fixed_value);
		}
	}
}

// interleave bits: (byte)76543210 -> (16 bit).7.6.5.4.3.2.1
static inline uint16_t interleave_bits(uint16_t value) {
	value = (value | (value << 4)) & 0x0f0f;
	value = (value | (value << 2)) & 0x3333;
	value = (value | (value << 1)) & 0x5555;
	return value;
}

// pixels on display are numbered from 1
void EPD_Class::all_pixels(const uint8_t *data, uint8_t fixed_value, bool read_progmem, EPD_stage stage) {
	for (uint16_t b = this->bytes_per_line; b > 0; --b) {
		if (NULL != data) {
#if !defined(__AVR__)
			uint8_t px = data[b - 1];
#else
			// AVR has multiple memory spaces
			uint8_t px;
			if (read_progmem) {
				px = pgm_read_byte_near(data + b - 1);
			} else {
				px = data[b - 1];
			}
#endif
			uint16_t pixels = interleave_bits(px);
			switch(stage) {
			case EPD_compensate:  // B -> W, W -> B (Current Image)
				pixels = 0xaaaa | (pixels ^ 0x5555);
				break;
			case EPD_white:       // B -> N, W -> W (Current Image)
				pixels = 0x5555 + (pixels ^ 0x5555);
				break;
			case EPD_inverse:     // B -> N, W -> B (New Image)
				pixels = 0x5555 | ((pixels ^ 0x5555) << 1);
				break;
			case EPD_normal:       // B -> B, W -> W (New Image)
				pixels = 0xaaaa | pixels;
				break;
			}
			SPI_put(pixels >> 8);
			SPI_put(pixels);
		} else {
			SPI_put(fixed_value);
			SPI_put(fixed_value);
		}
	}
}


void EPD_Class::nothing_frame() {
	for (int line = 0; line < this->lines_per_display; ++line) {
		this->line(0x7fffu, 0, 0x00, false, EPD_compensate);
	}
}


void EPD_Class::dummy_line() {
	this->line(0x7fffu, 0, 0x00, false, EPD_compensate);
}


void EPD_Class::border_dummy_line() {
	this->line(0x7fffu, 0, 0x00, false, EPD_normal);
}


// output one line of scan and data bytes to the display
void EPD_Class::line(uint16_t line, const uint8_t *data, uint8_t fixed_value, bool read_progmem, EPD_stage stage, ALTERWEAR_EFFECT effect=ALTERWEAR_DEFAULT){
	//Serial.print("alter-version, line... ");
	SPI_on();

	// send data
	Delay_us(10);
	SPI_send(this->EPD_Pin_EPD_CS, CU8(0x70, 0x0a), 2);
	Delay_us(10);

	// CS low
	digitalWrite(this->EPD_Pin_EPD_CS, LOW);
	SPI_put(0x72);

	if (this->pre_border_byte) {
		SPI_put(0x00);
	}

	// true for 2.0 and 1.44, false for 1.9
	if (this->middle_scan) {
		// w/ even_pixels called on top, and odd_pixels called below the loop,
		// the images are flipped
		//if odd_pixels is called first, the image is normal

		// note, cutting out the "break" statements makes a super insane plaid experience.
		switch(effect){
			case ALTERWEAR_FLIP:
				this->even_pixels(data, fixed_value, read_progmem, stage);
				break;
			case ALTERWEAR_HALF_FLIP:
				this->odd_pixels(data, fixed_value, read_progmem, stage);
				break;
			case ALTERWEAR_EVEN_ONLY:
				// only call even pixels
				break;
			case ALTERWEAR_EEPROM:
				// only call even pixels for now
				this->odd_pixels(data, fixed_value, read_progmem, stage);
				break;
			case ALTERWEAR_DEFAULT:
				this->odd_pixels(data, fixed_value, read_progmem, stage);
				break;
		}
		

		// scan line
		for (uint16_t b = this->bytes_per_scan; b > 0; --b) {
			uint8_t n = 0x00;
			if (line / 4 == b - 1) {
				n = 0x03 << (2 * (line & 0x03));
			}
			SPI_put(n);
		}

		// if the only call to even or odd pixels happens below the above loop
		// nothing happens.
		// if the call to one happens above, and the call to the other happens below,
		// everything is fine.
		switch(effect){
			case ALTERWEAR_FLIP:
				this->odd_pixels(data, fixed_value, read_progmem, stage);
				break;
			case ALTERWEAR_HALF_FLIP:
				this->odd_pixels(data, fixed_value, read_progmem, stage);
				break;
			case ALTERWEAR_EVEN_ONLY:
				this->even_pixels(data, fixed_value, read_progmem, stage);
				break;
			case ALTERWEAR_EEPROM:
				this->even_pixels(data, fixed_value, read_progmem, stage, effect);
				break;
			case ALTERWEAR_DEFAULT:
				this->even_pixels(data, fixed_value, read_progmem, stage);
				break;
		}

	} else {
		// even scan line, but as lines on display are numbered from 1, line: 1,3,5,...
		for (uint16_t b = 0; b < this->bytes_per_scan; ++b) {
			uint8_t n = 0x00;
			if (0 != (line & 0x01) && line / 8 == b) {
				n = 0xc0 >> (line & 0x06);
			}
			SPI_put(n);
		}

		// data bytes
		this->all_pixels(data, fixed_value, read_progmem, stage);

		// odd scan line, but as lines on display are numbered from 1, line: 0,2,4,6,...
		for (uint16_t b = this->bytes_per_scan; b > 0; --b) {
			uint8_t n = 0x00;
			if (0 == (line & 0x01) && line / 8 == b - 1) {
				n = 0x03 << (line & 0x06);
			}
			SPI_put(n);
		}
	}

	// post data border byte
	switch (this->border_byte) {
	case EPD_BORDER_BYTE_NONE:  // no border byte requred
		break;

	case EPD_BORDER_BYTE_ZERO:  // border byte == 0x00 requred
		SPI_put(0x00);
		break;

	case EPD_BORDER_BYTE_SET:   // border byte needs to be set
		switch(stage) {
		case EPD_compensate:
		case EPD_white:
		case EPD_inverse:
			SPI_put(0x00);
			break;
		case EPD_normal:
			SPI_put(0xaa);
			break;
		}
		break;
	}

       // CS high
       digitalWrite(this->EPD_Pin_EPD_CS, HIGH);

       // output data to panel
       SPI_send(this->EPD_Pin_EPD_CS, CU8(0x70, 0x02), 2);
       SPI_send(this->EPD_Pin_EPD_CS, CU8(0x72, 0x07), 2);

       SPI_off();
}


static void SPI_on(void) {
	SPI.end();
	SPI.begin();
	SPI.setBitOrder(MSBFIRST);
#if defined(__MSP432P401R__)
	SPI.setDataMode(SPI_MODE3);
	SPI.setClockDivider(SPI_CLOCK_DIV2);
#else
	SPI.setDataMode(SPI_MODE0);
	SPI.setClockDivider(SPI_CLOCK_DIV2);
#endif
	SPI_put(0x00);
	SPI_put(0x00);
	Delay_us(10);
//	spi_is_on = true;
}


static void SPI_off(void) {
	// SPI.begin();
	// SPI.setBitOrder(MSBFIRST);
	SPI.setDataMode(SPI_MODE0);
	// SPI.setClockDivider(SPI_CLOCK_DIV2);
	SPI_put(0x00);
	SPI_put(0x00);
	Delay_us(10);
	SPI.end();
}


static void SPI_put(uint8_t c) {
	SPI.transfer(c);
}


static void SPI_send(uint8_t cs_pin, const uint8_t *buffer, uint16_t length) {
	// CS low
	digitalWrite(cs_pin, LOW);

	// send all data
	for (uint16_t i = 0; i < length; ++i) {
		SPI_put(*buffer++);
	}

	// CS high
	digitalWrite(cs_pin, HIGH);
}

#define DEBUG_SPI_READ 0
static uint8_t SPI_read(uint8_t cs_pin, const uint8_t *buffer, uint16_t length) {
	// CS low
	digitalWrite(cs_pin, LOW);

#if DEBUG_SPI_READ
	uint8_t rbuffer[16];
#endif
	uint8_t result = 0;

	// send all data
	for (uint16_t i = 0; i < length; ++i) {
		result = SPI.transfer(*buffer++);
#if DEBUG_SPI_READ
		if (i < sizeof(rbuffer)) {
			rbuffer[i] = result;
		}
#endif
	}

	// CS high
	digitalWrite(cs_pin, HIGH);

#if DEBUG_SPI_READ
	Serial.print("SPI read:");
	for (uint16_t i = 0; i < sizeof(rbuffer) && i < length; ++i) {
		Serial.print(" ");
		Serial.print(rbuffer[i], HEX);
	}
	Serial.println();
#endif

	return result;
}
