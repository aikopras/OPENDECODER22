//*****************************************************************************************************
//
// OpenDCC - OpenDecoder2.2
//
// Copyright (c) 2011,2013 Pras
//
// This source file is subject of the GNU general public license 2,
// that is available at the world-wide-web at http://www.gnu.org/licenses/gpl.txt
// 
//*****************************************************************************************************
//
// file:      switch_feedback.c
// author:    Aiko Pras
// history:   2013-12-31 V0.1 ap Initial version (based on earlier rs_bus_port.c)
//            2015-01-06 V0.2 ap Since switches are now counted from left to right, the order of
//                               needed to be changes as well. In addition, in case of SkipUnEven
//                               feedback bits of even AND uneven switches are now returned
//
//
// Routines for determining switch positions, which will be send via RS-Bus feedback messages
// to a LENZ Command Station
//
//
//*****************************************************************************************************
#include <stdlib.h>
#include <stdbool.h>
#include <inttypes.h>
#include <avr/pgmspace.h>	// put var to program memory
#include <avr/io.h>		// needed for UART
#include <avr/eeprom.h>
#include <avr/interrupt.h>
#include <util/parity.h>	// Needed to calculate the parity bit

#include "global.h"		// global variables
#include "config.h"		// general definitions the decoder, cv's
#include "myeeprom.h"           // wrapper for eeprom
#include "hardware.h"		// port definitions for target
#include "rs_bus_hardware.h"	// hardware related RS-bus functions (layer 1 / physical layer)
#include "rs_bus_messages.h"	// RS-bus layer 2 functions / defines of bit positions

#include "main.h"

//************************************************************************************************
// Define global variables
//************************************************************************************************
unsigned char skip_uneven_addresses; // use only even addresses, to solve potential feedback issues
unsigned char RS_tranmissions;       // number of times a RS-bus message is transmitted
struct
  {
    unsigned char samples;	     // buffer to store 8 consequtive samples, to filter glitches
    unsigned char previous_position; // this position has previously been send to the master
    unsigned char next_position;     // this position will be send to the master
    unsigned char stable;	     // 0: samples are different / 1: all samples are equal
    unsigned char to_send;	     // 0: no changes => no need to send 
                                     // >1: changed => needs to be send / Note: a higher value is
    				     // used to transmit multiple times (forward error correction)
  } feedback[8];		     // we have eight feedback signals (two per switch)
 
 
//************************************************************************************************
// init_switch_feedback will be directly called from main externally
//************************************************************************************************
void init_switch_feedback(void)
{ // Step 1: check if a nibble should hold feedbacks for a single switch, or for two switches
  // If the nibble should hold info for a single switch only, even addresses should not be used 
  skip_uneven_addresses = my_eeprom_read_byte(&CV.SkipUnEven);   // CV21                
  // Step 2: Determine number of times the same RS-feedback nibble will be transmitted
  // Minimum is 1, but if CV537 > 0 the nibble will be retransmitted (forward error correction)
  RS_tranmissions = 1 + my_eeprom_read_byte(&CV.RSRetry);   // CV20  
  // Step 3: Make RS_Addr2Use zero. Will be set by the RS_connect / send_feedbacks routines
  RS_Addr2Use = 0; 
}


//************************************************************************************************
// Next routines are used to read the values from the eigth feedback pins,
// to test if all pins are stable, to test if some pins are changed and to save the changes
//************************************************************************************************
void read_feedbacks(void)
{
  unsigned char new_sample, i;
  // Basic operation: maintain, per feedback signal, eight consequtive samples. If all these
  // samples have the same value (0x00 or 0xFF), the feedback signal is considered to be stable.
  // Step 1: Shift the bits in each of the eigth feedback variables one to the left.
  // In this way we collect 7 consequtive samples (note: newest bit at the right will be zero). 
  for (i = 0; i < 8; i++) { feedback[i].samples = (feedback[i].samples << 1);}
  // Step 2: Give the newest bit at the right a value. Read therefore the input port for feedbacks
  new_sample = FEEDBACK_IN;
  feedback[0].samples |= ((new_sample & 0x01) >> 0);	// bit 0
  feedback[1].samples |= ((new_sample & 0x02) >> 1);	// bit 1
  feedback[2].samples |= ((new_sample & 0x04) >> 2);	// bit 2
  feedback[3].samples |= ((new_sample & 0x08) >> 3);	// bit 3
  feedback[4].samples |= ((new_sample & 0x10) >> 4);	// bit 4
  feedback[5].samples |= ((new_sample & 0x20) >> 5);	// bit 5
  feedback[6].samples |= ((new_sample & 0x40) >> 6);	// bit 6
  feedback[7].samples |= ((new_sample & 0x80) >> 7);	// bit 7
  // Step 3: Check if all samples of each feedback signal are stable (samples are 0x00 or 0xFF)
  // If they have the same value, and if this value is different from the value that was previously
  // been send to the master, we set the changed flag and fill in the next_position
  for (i = 0; i < 8; i++) 
  {
    if ((feedback[i].samples == 0x00) || (feedback[i].samples == 0xFF))
    {
      feedback[i].stable = 1;
      if ((feedback[i].samples == 0x00) && (feedback[i].previous_position != 0x00))
      {
        feedback[i].next_position = 0x00;
        feedback[i].to_send = RS_tranmissions;
      }
      if ((feedback[i].samples == 0xFF) && (feedback[i].previous_position == 0x00))
      {
        feedback[i].next_position = 0x01;
        feedback[i].to_send = RS_tranmissions;
      }
    }
    else {feedback[i].stable = 0;}
  }
}


unsigned char stable(unsigned char start, unsigned char end)
{
  // The previous function "read_feedbacks" tested stability for each individual feedback signal
  // This function tests if all feedback signals between "start" and "end" are stable
  unsigned char i, result;
  result = 1;                         // initial assumption: all feedback signals are stable
  for (i = start; i <= end; i++) {    // does this assumption hold for all feedback signals?
    if (feedback[i].stable != 1) {result = 0;} }
  return result;
}


unsigned char changed(unsigned char start, unsigned char end)
{
  // The function "read_feedbacks" tested and set "changed" for each individual feedback signal
  // Clearing "changed" is done by the functions responsible for transmission via the RS-bus
  // This function tests if one or more feedback signals between "start" and "end" have changed
  unsigned char i, result;
  result = 0;                         // initial assumption: no feedback signal has changed
  for (i = start; i <= end; i++) {    // does this assumption hold for all feedback signals?
    if (feedback[i].to_send > 0) {result = 1;} }
  return result;
}


void save_changes(unsigned char start, unsigned char end)
{
  // This function saves the changes for the feedbacks between "start" and "end"
  // after the nibble to which they belong has been send to the master
  unsigned char i;
  for (i = start; i <= end; i++) { 
    feedback[i].previous_position = feedback[i].next_position;
    if (feedback[i].to_send > 0)   {feedback[i].to_send --;} }
}


//************************************************************************************************
// The next routine is used to connect to the master
//************************************************************************************************
void RS_connect(void)
{
  // Register feedback module: send low and high order nibble in 2 consequtive cycles
  // In case we use two addresses, we should repeat this for the second address
  // Note that interference on the AVR's input lines during AVR restart requires us 
  // to wait till all feedback signals have become stable. Note that spurious resets on the
  // AVR 644A have also been detected (the AVR signals "brown-out" reset, although no power
  // problems can be measured), which means that the AVR can also be restarted during normal
  // operation. Therefore we have to make sure we always send correct (thus stable) values.
  unsigned char nibble;
  if ((RS_Layer_1_active) && stable(0,7))  // wait till RS-bus is active and feedback stable
  {
    if (skip_uneven_addresses)
    { // We use two feedback addresses or, in case of switches, 8 (instead of 4) switch addresses
      // send first nibble, first address
      while (RS_data2send_flag) {};	// busy wait, till the USART ISR has send previous data
      RS_Addr2Use = My_RS_Addr;
      nibble = (feedback[0].next_position<<DATA_1)
             | (feedback[1].next_position<<DATA_0)
             | (0<<DATA_3)
             | (0<<DATA_2)
             | (0<<NIBBLE);
      save_changes(0,1);
      format_and_send_RS_data_nibble(nibble);      
      // send second nibble, first address
      while (RS_data2send_flag) {};	// busy wait, till the USART ISR has send previous data
      RS_Addr2Use = My_RS_Addr;
      nibble = (feedback[2].next_position<<DATA_1)
             | (feedback[3].next_position<<DATA_0)
             | (0<<DATA_3)
             | (0<<DATA_2)
             | (1<<NIBBLE);             
      save_changes(2,3);
      format_and_send_RS_data_nibble(nibble);
      // send first nibble, second address
      while (RS_data2send_flag) {};	// busy wait, till the USART ISR has send previous data
      RS_Addr2Use = My_RS_Addr + 1;
      nibble = (feedback[4].next_position<<DATA_1)
             | (feedback[5].next_position<<DATA_0)
             | (0<<DATA_3)
             | (0<<DATA_2)
             | (0<<NIBBLE);
      save_changes(4,5);
      format_and_send_RS_data_nibble(nibble);      
      // send second nibble, second address
      while (RS_data2send_flag) {};	// busy wait, till the USART ISR has send previous data
      RS_Addr2Use = My_RS_Addr + 1;
      nibble = (feedback[6].next_position<<DATA_1)
             | (feedback[7].next_position<<DATA_0)
             | (0<<DATA_3)
             | (0<<DATA_2)
             | (1<<NIBBLE);             
      save_changes(6,7);
      format_and_send_RS_data_nibble(nibble);
    }
    else
    { // we use a single feedback address for all 8 feedback signals (thus four switches)
      // send first nibble
      while (RS_data2send_flag) {};	// busy wait, till the USART ISR has send previous data
      RS_Addr2Use = My_RS_Addr;
      nibble = (feedback[0].next_position<<DATA_1)
             | (feedback[1].next_position<<DATA_0)
             | (feedback[2].next_position<<DATA_3)
             | (feedback[3].next_position<<DATA_2)
             | (0<<NIBBLE);
      save_changes(0,3);
      format_and_send_RS_data_nibble(nibble);      
      // send second nibble
      while (RS_data2send_flag) {};	// busy wait, till the USART ISR has send previous data
      RS_Addr2Use = My_RS_Addr;
      nibble = (feedback[4].next_position<<DATA_1)
             | (feedback[5].next_position<<DATA_0)
             | (feedback[6].next_position<<DATA_3)
             | (feedback[7].next_position<<DATA_2)
             | (1<<NIBBLE);             
      save_changes(4,7);
      format_and_send_RS_data_nibble(nibble);
    }
    RS_Layer_2_connected = 1;		// This module should now be connected to the master station
  }
}


//************************************************************************************************
// Next routines are used to send the values from the eigth feedback pins
//************************************************************************************************
void send_feedbacks(void)
{ // We send a feedback nibble to the master station if:
  // 1: the USART has completed transmission of the previous data
  // 2A: all feedback signals belonging to that nibble are stable, and
  // 2B: at least one of the feedback signals of that nibble has changed
  // Depending on whether we "skip_uneven_addresses", the nibble contains the feedback signals
  // of one switch (we skip uneven addresses), or two switches (we do not skip addresses).
  // With "slow switches", such as those driven by a motor, the first feedback signal may be
  // changed a few seconds before the second is changed. As a consequence, for a limited period,
  // both feedback signals of a switch (switch is red / green) may be high (or low).
  // After a short time, however, only one of them will remain high (or low).
  unsigned char nibble;
  // check if we may send data (thus the USART has completed transmission of the previous data)
  if (RS_data2send_flag == 0) { 
    // Check if we use 2 feedback addresses or, for switches, 8 (instead of 4) switch addresses
    if (skip_uneven_addresses) { 
      if (stable(0,1) && changed(0,1)) {
        RS_Addr2Use = My_RS_Addr;
        nibble = (feedback[0].next_position<<DATA_1)
               | (feedback[1].next_position<<DATA_0)
               | (feedback[0].next_position<<DATA_3)
               | (feedback[1].next_position<<DATA_2)
               | (0<<NIBBLE);
        save_changes(0,1);
        format_and_send_RS_data_nibble(nibble);
      } // if (stable(0,1)  ...
      else if (stable(2,3) && changed(2,3)) {
        RS_Addr2Use = My_RS_Addr;
        nibble = (feedback[2].next_position<<DATA_1)
               | (feedback[3].next_position<<DATA_0)
               | (feedback[2].next_position<<DATA_3)
               | (feedback[3].next_position<<DATA_2)
               | (1<<NIBBLE);             
        save_changes(2,3);
        format_and_send_RS_data_nibble(nibble);
      } // if (stable(2,3)  ...
      else if (stable(4,5) && changed(4,5)) {
        RS_Addr2Use = My_RS_Addr + 1;
        nibble = (feedback[4].next_position<<DATA_1)
               | (feedback[5].next_position<<DATA_0)
               | (feedback[4].next_position<<DATA_3)
               | (feedback[5].next_position<<DATA_2)
               | (0<<NIBBLE);             
        save_changes(4,5);
        format_and_send_RS_data_nibble(nibble);
      } // if (stable(4,5)  ...
      else if (stable(6,7) && changed(6,7)) {
        RS_Addr2Use = My_RS_Addr + 1;
        nibble = (feedback[6].next_position<<DATA_1)
               | (feedback[7].next_position<<DATA_0)
               | (feedback[6].next_position<<DATA_3)
               | (feedback[7].next_position<<DATA_2)
               | (1<<NIBBLE);             
        save_changes(6,7);
        format_and_send_RS_data_nibble(nibble);
      } // if (stable(6,7)  ...
    } // if (skip_uneven_addresses)
    else
    { // we use a single feedback address for all 8 feedback signals (thus four switches)
      if (stable(0,3) && changed(0,3)) {
        RS_Addr2Use = My_RS_Addr;
        nibble = (feedback[0].next_position<<DATA_1)
               | (feedback[1].next_position<<DATA_0)
               | (feedback[2].next_position<<DATA_3)
               | (feedback[3].next_position<<DATA_2)
               | (0<<NIBBLE);
        save_changes(0,3);
        format_and_send_RS_data_nibble(nibble);
      } // if (stable(0,3)  ...
      else if (stable(4,7) && changed(4,7)) {
        RS_Addr2Use = My_RS_Addr;
        nibble = (feedback[4].next_position<<DATA_1)
               | (feedback[5].next_position<<DATA_0)
               | (feedback[6].next_position<<DATA_3)
               | (feedback[7].next_position<<DATA_2)
               | (1<<NIBBLE);             
        save_changes(4,7);
        format_and_send_RS_data_nibble(nibble);
      } // else if stable(4,7)  ...
    }   // else  
  }     // if (RS_data2send_flag == 0)
}


//************************************************************************************************
// This routine is called from main every 20 milliseconds (to avoid processor overload / debounce)
//************************************************************************************************
void send_switch_feedback(void)
{ // Check if My_RS_Addr is initialised
  if (My_RS_Addr > 0) {
    read_feedbacks();   
    if (RS_Layer_2_connected == 0) {RS_connect();}	// Try to connect
      else {send_feedbacks();}
  }
}


