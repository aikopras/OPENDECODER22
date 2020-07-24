//************************************************************************************************
//
// file:      rs_bus_messages.c
//
// purpose:   Routines for the RS messages feedback bus routines
//
// This source file is subject of the GNU general public license 2,
// that is available at http://www.gnu.org/licenses/gpl.txt
//
// history:   2010-11-10 V0.1 Initial version
//            2013-04-20 V0.2 Only send routines kept - derived from previolus rs_bus_port.h
//
// This code can be used to send feedback information from decoder to master station via
// the RS-bus. This code implements the datalink layer routines (define the byte contents).
// For details on the operation of the RS-bus, see: 
// http://www.der-moba.de/index.php/RS-R%C3%BCckmeldebus
//
// Calling:
// - format_and_send_RS_data_nibble(value) is called occupancy.c 
// - send_CV_value_via_RSbus (value) is called from cv_pom.c
// 
// Input data is provided as parameter in the above functions
//
//************************************************************************************************

#include <stdlib.h>
#include <stdbool.h>
#include <inttypes.h>
#include <util/parity.h>	// Needed to calculate the parity bit

#include "global.h"             // global variables

#include "led.h"                // LED specific functions
#include "rs_bus_hardware.h"	// hardware related RS-bus functions (layer 1 / physical layer)



//************************************************************************************************
// The next routine is used to format and send a RS data byte (a feedback nibble)
//************************************************************************************************
void format_and_send_RS_data_nibble(unsigned char value) {
  // This routine "formats" and "sends" the RS-bus byte.
  // Input is a byte, representing 4 feedback bits, plus one nibble bit
  // 1) the routine sets the TT and parity bits
  // 2) it calls the routine that sends the data
  // Note: the following kind of RS-bus modules exist (see also http://www.der-moba.de/):
  // - 0: accessory decoder without feedback
  // - 1: accessory decoder with RS-Bus feedback (this would be the normal case)
  // - 2: feedback module for the RS-Bus (can not switch anything)
  // - 3: Reserved for future use
  unsigned char parity;
  // Step 1A: set the "type" bit
  if (RS_BUS_TYPE == 0) {value |= (1<<TT_BIT_0) | (0<<TT_BIT_1);}	// switch decoder with feedback
  if (RS_BUS_TYPE == 1) {value |= (0<<TT_BIT_0) | (1<<TT_BIT_1);}	// feedback module
  // Step 1B: Set the parity bit
  parity = parity_even_bit(value);			// check parity
  if (parity)						// if parity is even
    {value |= (0<<PARITY);}				// clear the parity bit
    else {value |= (1<<PARITY);}			// set the parity bit
  // Step 2: send a formatted data byte over the RS-bus.
  // It copies the formatted "data byte" into the RS_data2send interface variable; 
  // this data in the interface variable will be send via the USART by the INT0 ISR. 
  RS_data2send = value;			      	  	// this byte will be send by the USART
  RS_data2send_flag = 1;				// the USART ISR may now send the byte
  feedback_led();					// Indicate via the LED that we send someting
}


//************************************************************************************************
// Next routine is used to send CV values back after a PoM read request via the RS-bus
//************************************************************************************************
void send_CV_value_via_RSbus(unsigned char value)
{ // Send the 8 bit value in two consecutive nibbles. Note that bit order should be changed.
  unsigned char nibble;
  
  RS_Addr2Use = 128;
  
  
  // check if we may send data (thus the USART has completed transmission of the previous data)
  if (RS_data2send_flag == 0) 
  { // send first nibble (for the low order bits)
    nibble = ((value & 0b00000001) <<7)  // move bit 7 to bit 0 (distance = 7)
           | ((value & 0b00000010) <<5)  // move bit 6 to bit 1 (distance = 5)
           | ((value & 0b00000100) <<3)  // move bit 5 to bit 2 (distance = 3)
           | ((value & 0b00001000) <<1)  // move bit 4 to bit 3 (distance = 1)
           | (0<<NIBBLE);
    format_and_send_RS_data_nibble(nibble);
    while (RS_data2send_flag) {};	 // busy wait, till the USART ISR has send previous data
    // send second nibble (for the high order bits)
    nibble = ((value & 0b00010000) <<3)  // move bit 3 to bit 0 (distance = 3)
           | ((value & 0b00100000) <<1)  // move bit 2 to bit 1 (distance = 1)
           | ((value & 0b01000000) >>1)  // move bit 1 to bit 2 (distance = -1)
           | ((value & 0b10000000) >>3)  // move bit 0 to bit 3 (distance = -3)
           | (1<<NIBBLE);
    format_and_send_RS_data_nibble(nibble);
  } 
}


//------------------------------------------------------------------------------------------------

