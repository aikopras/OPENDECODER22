//*****************************************************************************************************
//
// OpenDCC - OpenDecoder2.2
//
// Copyright (c) 2006, 2007 Kufer
// Copyright (c) 2011, 2012, 2013 AP
//
// This source file is subject of the GNU general public license 2,
// that is available at http://www.gnu.org/licenses/gpl.txt
// 
//*****************************************************************************************************
//
// file:      config.c
// author:    Wolfgang Kufer / Aiko Pras
// history:   2007-02-25 V0.01 kw start
//            2014-01-06 V0.02 ap Modified, to select the correct default settings for this decoder
//
//*****************************************************************************************************
//
// purpose:   DCC Switch / Relays decoder 
//            here: global variables (RAM and EEPROM)
//
// ===== >>>> config.h  is the central definition file for the project
// ===== >>>> global.c  see also tglobal.c for the definition of global variables
//
//            Note: all eeprom variables must be allocated here
//            (reason: AVRstudio doesn't handle eeprom directives
//            correctly)
//
//
//*****************************************************************************************************

#include <stdlib.h>
#include <stdbool.h>
#include <inttypes.h>
#include <avr/pgmspace.h>
#include <avr/io.h>
#include <avr/eeprom.h>
#include <avr/interrupt.h>

#include "config.h"
#include "global.h"		// Definition of DECODER TYPES

//----------------------------------------------------------------------------
// Timing Definitions:
// (note: every timing is given in us)

#define TICK_PERIOD 20000L       // 20ms tick for Timing Engine
                                 // => possible values for timings up to
                                 //    5.1s (=255/0.020)
                                 // note: this is also used as frame for
                                 // Servo-Outputs (OC1A and OC1B)

//----------------------------------------------------------------------------
// Global Data


volatile signed char timerval;          // generell timer tick, this is incremented
                                        // by Timer-ISR, wraps around. 1 Tick = 20 ms

volatile unsigned char timer1fired; // Indicates timer 1 has fired


volatile unsigned char Communicate = 0; // Communicationregister (for semaphors)
   

//-----------------------------------------------------------------------------
// data in EEPROM:
// Note: the order of these data corresponds to physical CV-Address
//       CV1 is coded at #00
//       see RP 9.2.2 for more information


#if (DECODER_TYPE == TYPE_SWITCH) 
    const unsigned char compilat[] PROGMEM = {".... SWITCH ...."};

    t_cv_record CV EEMEM = { 
	#include "cv_data_switch.h"
    };

    const t_cv_record CV_PRESET PROGMEM = {
	#include "cv_data_switch.h"
    };


#elif (DECODER_TYPE == TYPE_RELAYS4)
    const unsigned char compilat[] PROGMEM = {".... RELAYS4 ...."};

    t_cv_record CV EEMEM = { 
	#include "cv_data_relays4.h"
    };

    const t_cv_record CV_PRESET PROGMEM = {
	#include "cv_data_relays4.h"
    };


#else
#warning DECODER_TYPE NOT DEFINED!

#endif





