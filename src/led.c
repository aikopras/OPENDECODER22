//------------------------------------------------------------------------
//
// OpenDCC - OpenDecoder2
//
// Copyright (c) 2006,2007 Kufer
//
// This source file is subject of the GNU general public license 2,
// that is available at the world-wide-web at
// http://www.gnu.org/licenses/gpl.txt
// 
//------------------------------------------------------------------------
//
// file:      led.c
// author:    Wolfgang Kufer
// history:   2007-02-20 V0.1 kw copied from opendecoder.c
//                               removed code for versatile output -
//                               we only do turnout commands and permanent
//                               commands (up to now)
//            2011-12-31 V0.2 ap Removed everything, except the timer related code
//            2013-04-03 V0.3 ap Removed everything, except the LED related code
//
//------------------------------------------------------------------------
//
// purpose:   flexible general purpose decoder for dcc
//            here: led flashing
//
//------------------------------------------------------------------------

#include <stdlib.h>
#include <stdbool.h>
#include <inttypes.h>
#include <avr/pgmspace.h>
#include <avr/io.h>
#include <avr/eeprom.h>
#include <avr/interrupt.h>
#include <string.h>

#include "global.h"
#include "config.h"
#include "hardware.h"
#include "led.h"

//*****************************************************************************************************
//***************************************** LED specific code *****************************************
//*****************************************************************************************************
// Possible LED modes
#define ALWAYS_OFF 0 		// LED is off
#define FLASH_ONCE 1 		// LED flashes just a single time
#define FLASH_CONT 2 		// LED keeps flashing, till it is explicitly switch off
#define ALWAYS_ON  3 		// LED remains on, till it is explicitly switch off

struct
{
  unsigned char mode;     	// See defines above
  unsigned char rest;     	// Remaining time before LED status changes. In Ticks (20ms)
  unsigned char ontime;   	// LED on time
  unsigned char offtime;  	// LED off time
  unsigned char pause;    	// Longer LED off time: between a series of flashes
  unsigned char flashes;  	// Number of flashes before a longer LED off time
  unsigned char act_flash;	// Number of flashes thusfar
} led;


void turn_led_on(void) {
  led.mode = ALWAYS_ON;
  LED_ON;
}

void turn_led_off(void) {
  led.mode = ALWAYS_OFF;
  LED_OFF;
}

// Single short flash, to indicate a RS-Bus feedback
void feedback_led(void) {
  led.mode = FLASH_ONCE;			// single flash
  led.rest    = 80000L / TICK_PERIOD;		// 0,08 sec
  LED_ON;
}


// Single very short flash, to indicate a switch command
void activity_led(void) {
  led.mode = FLASH_ONCE;			// single flash
  led.rest    = 40000L / TICK_PERIOD;		// 0,08 sec
  LED_ON;
}


// make a series of flashes, then a longer pause
void flash_led_fast(unsigned char count) {
  led.mode = FLASH_CONT;			// Continue flashing, till turn_led_off()
  led.flashes = count;				// Number of flashes till pause
  led.act_flash = 1;				// This is the first flash
  led.pause   = 700000L / TICK_PERIOD;		// 0,70 sec
  led.offtime = 240000L / TICK_PERIOD;		// 0,24 sec
  led.ontime  = 120000L / TICK_PERIOD;		// 0,12 sec
  led.rest    = 120000L / TICK_PERIOD;		// 0,12 sec
  LED_ON;
}


// the next function will be called from main every 20 seconds
void check_led_time_out(void) {
  if (led.mode == ALWAYS_ON) return;
  if (led.mode == ALWAYS_OFF) return;
  if (led.mode == FLASH_ONCE) {			// Single Flash
    if (--led.rest == 0) {			// Do we need to change state?
      led.mode = ALWAYS_OFF;
      LED_OFF;
    }
  }
  else if (led.mode == FLASH_CONT) {		// Continuous Flash 
    if (--led.rest == 0) {			// Do we need to change state?
      if (LED_STATE) {				// LED is currently ON (AVR Pin for LED is high)
        if (led.act_flash == led.flashes) {	// We did flash the required number of times
          led.rest = led.pause;			// Next will be a longer pause
          led.act_flash = 0;			// Restart the counter for the required number of flashes
        }
        else {					// We still need to perform a number of flashes
          led.rest = led.offtime;		// Next will be a short pause
        }
        LED_OFF;				// Turn LED off
      }
      else {					// LED is OFF
        led.act_flash++;			// Increment the number of blinks we did
        led.rest = led.ontime;			// Next will be a certain time on
        LED_ON;					// Turn LED on
      }
    }
  }
}
