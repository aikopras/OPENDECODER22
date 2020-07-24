//*****************************************************************************************************
//
// OpenDCC - OpenDecoder2
//
// Copyright (c) 2006, 2007 Kufer
//
// This source file is subject of the GNU general public license 2,
// that is available at the world-wide-web at
// http://www.gnu.org/licenses/gpl.txt
// 
//*****************************************************************************************************
//
// file:      dcc_decode.c
// author:    Wolfgang Kufer
// contact:   kufer@gmx.de
// webpage:   http://www.opendcc.de
// history:   2007-02-14 V0.1 kw copied from opendecoder.c
//                               split up the code in modules
//                               added routines for CV-Programming
//            2007-03-30 V0.2 kw corrected some items with Decoding
//            2007-04-16 V0.3 kw added debugswitch for service mode and
//                               feedback testing
//                               service mode is only executed after second
//                               received command
//            2007-04-27 V0.4 kw changed return codes
//            2007-08-04 V0.5 kw Bugfix in cv_is_blocked
//            2007-08-18 V0.6 kw masking of myADDRHigh with 0x7F
//                               (hidden bit: unprogrammed)
//            2007-11-22 V0.7 kw Decoder Reset added        
//            2011-01-15 v0.8 ap In "analyze_message /  Basic Accessory handling" a new test
//                               is included to correct "errors" made by LENZ LZV100 master
//                               stations. This test is controlled by a new CV. 
//                               Although this correction will not be needed if the decoder is used
//                               in the traditional way (controlling four switches via four 
//                               subsequent addresses, no RS-bus feedback), it will improve
//                               operation if it is used together with LENZ LZV100 master stations. 
//            2012-01-07 v0.9 ap Various changes, to allow PoM using a loco adress instead
//                               The loco address is the feedback decoder address (RS-bus address)
//                               plus a certain offset
//                               Only via this trick it is possible to modify the CVs
//            2013-03-23 v0.A ap Many changes / complete restructure
//                               Gloval variables are moved to global.h
//                               Returns with accessory data, PoM data or F1..F4 data 
//                               PoM is moved to cv_pom.c 
//
//
// purpose:   flexible general purpose decoder for dcc
//
// required:  a running timerengine (for timeouts)
//            this engine is currently implemented in timer1_led
//            (TICK_PERIOD, timerval)
//
//*****************************************************************************************************

#include <stdlib.h>
#include <stdbool.h>
#include <inttypes.h>
#include <avr/pgmspace.h>        // put var to program memory
#include <avr/io.h>
#include <avr/eeprom.h>
#include <avr/interrupt.h>
#include <string.h>

#include "global.h"              // global variables
#include "config.h"              // general definitions the decoder, cv's
#include "myeeprom.h"            // wrapper for eeprom
#include "hardware.h"            // port definitions

#include "dcc_receiver.h"        // receiver for dcc
#include "dcc_decode.h"          // decoder for dcc

#include "rs_bus_hardware.h"     // 
#include "rs_bus_messages.h"     // for sending RS-bus feedback messages (after POM)
#include "timer1.h"              // For the LED blinking routine 
#include "cv_pom.h"         	 // CV programming

#include "lcd.h"		 // Peter Fleury's LCD routines
#include "lcd_ap.h"		 // Included by AP for debugging purposes


#define SERVICE_MODE_TIMEOUT   40000L    // 40ms - at least 20ms
#if ((SERVICE_MODE_TIMEOUT / TICK_PERIOD) == 0)
  #warning: TICK_PERIOD too large
#endif
#if ((SERVICE_MODE_TIMEOUT / TICK_PERIOD) > 127)
  #error: TICK_PERIOD too small
#endif


//***************************************************************************************
// Local data
//***************************************************************************************
unsigned char service_mode_state;	// bit field
#define SM_ENABLED   0			// Bit 0: 0: normal operation
					//        1: service mode
#define SM_RECEIVED  1			// Bit 1: 0: initial state
					//        1: there is already a received SM
                          
signed char last_sm_mode_received;	// timer variable to create a update grid;

unsigned int  MyFirstAdrPlusCoil;	// First "global" address this decoder listens to
unsigned int  MyLastAdrPlusCoil;	// "global" address = switch address LH100 - 1
unsigned char RecDecPort;	 	// Two bit port number as contained in the received DCC packet.
unsigned char RecF1_F4;			// Received value of F1..F4
unsigned char LastRecF1_F4;    	 	// Bit0=F1, Bit1=F2, Bit2=F3, Bit3=F4 
					// If 255: we are not yet initialized

unsigned int  MyFirstLocoAddr;		// First LOCO address this decoder listens to
unsigned int  MyLastLocoAddr;		// Last LOCO address this decoder listens to


//***************************************************************************************
// Service Mode message (programming on the special programming track)
//***************************************************************************************
// Service Mode is not supported for GBM decoders, since they are powered from the track
// The code below has not been tested after the complete restructure

// A Digital Decoder will enter service mode upon receipt of a valid service mode
// instruction packet immediately proceeded by a reset packet.
// A Digital Decoder will exit service mode but not enter operations mode
// when any of the following conditions are true:
//   1) The Digital Decoder successfully receives a non service mode instruction packet.
//   2) 20 milliseconds have occurred since the last valid reset or service mode
//      instruction packet was received.
// Once a Digital Decoder has exited service mode, it will only re-enter operations mode
// upon receipt of a valid operations mode packet that is not identical to a service mode
// packet. This is to ensure that the decoder does not start executing service mode
// instruction packets as operations mode packets
// (Service Mode instruction packets have a short address in the range of 112 to 127 decimal.)
unsigned char analyze_service_mode_message(t_message *new_dcc)
{
  if ((char)(timerval - last_sm_mode_received) >= (SERVICE_MODE_TIMEOUT / TICK_PERIOD))
  {
    service_mode_state = 0;                    // timeout reached, leave service mode
  }
  if (new_dcc->dcc[0] == 0)
  {
    if (new_dcc->dcc[1] == 0)
    { // reset message - enter service mode
      service_mode_state = (1 << SM_ENABLED);
      last_sm_mode_received = timerval;
      return(IGNORE_CMD);
    }
  }
  else if ((new_dcc->dcc[0] >= 112) && (new_dcc->dcc[0] <= 127))
  {
    if (new_dcc->size == 4) // direct mode
    {
      service_mode_state |= (1 << SM_ENABLED);
      last_sm_mode_received = timerval;
      // direct mode
      // {preamble} 0 0111CCAA 0 AAAAAAAA 0 DDDDDDDD 0 EEEEEEEE 1
      // CC = 11: write
      // CC = 01: verify
      // CC = 10: bit op
      // {preamble} 0 0111CCAA 0 AAAAAAAA 0 111KDBBB 0 EEEEEEEE 1
      //  K = (1=write, 0=verify) D = Bitvalue, BBB = bitpos
      if (service_mode_state & (1 << SM_RECEIVED))
      {  // this is the second message
        unsigned int tempi;
        unsigned char tempc;
        tempc = (new_dcc->dcc[0] & 0b00001100);// CC bits
        tempc = tempc >> 2; // CC bits
        
        tempi = ((new_dcc->dcc[0] & 0b00000011) << 8)
        | new_dcc->dcc[1];
        
        if ( (tempc == RecCvOperation) &&
            (tempi == RecCvNumber) &&
            (new_dcc->dcc[2] == RecCvData)  )
        {
          return (SM_CMD);
          // cv_operation();
        }
        service_mode_state &= ~(1 << SM_RECEIVED);
      }
      else
      {
        service_mode_state |= (1 << SM_RECEIVED);   // we have a sm message
        RecCvOperation = (new_dcc->dcc[0] & 0b00001100);  // CC bits
        RecCvOperation = RecCvOperation >> 2;
        RecCvNumber = ((new_dcc->dcc[0] & 0b00000011) << 8) | new_dcc->dcc[1];
        RecCvData = new_dcc->dcc[2];
      }
    }
    if (new_dcc->size == 3) // paged/register mode
    {
      service_mode_state |= (1 << SM_ENABLED);
      last_sm_mode_received = timerval;
      // paged/register mode
      // {preamble} 0 0111CRRR 0 DDDDDDDD 0 EEEEEEEE 1
      // C = 1: write
      // C = 0: verify
      // RRR = Register
      // !!!! Note: this is not supported !!!!
    }
    return(IGNORE_CMD);
  }
  else if (new_dcc->dcc[0] == 255)
  {
    last_sm_mode_received = timerval;
    return(IGNORE_CMD);
  }
  return(IGNORE_CMD);
}


//***************************************************************************************
// Broadcast Command for Multi Function Digital Decoders
//***************************************************************************************
unsigned char analyze_broadcast_message(t_message *new_dcc)
{ if (new_dcc->dcc[1] == 0)
  { service_mode_state |= (1 << SM_ENABLED);
    last_sm_mode_received = timerval;
  }
  return(IGNORE_CMD);
}

//***************************************************************************************
// Multi-Function (LOCO) decoders with 7 and 14 bit addresses
//***************************************************************************************
// Support function that checks whether a function (F1..F4) has changed
// If yes, it sets TargetDevice, TargetDevice and TargetActivate
unsigned char function_changed(unsigned char mask) {
  if ((RecF1_F4 & mask) != (LastRecF1_F4 & mask)) { // changed
    // Set TargetDevice
    if (mask == 0b00000001) TargetDevice = 0;
    if (mask == 0b00000010) TargetDevice = 1;
    if (mask == 0b00000100) TargetDevice = 2;
    if (mask == 0b00001000) TargetDevice = 3;
    // Set TargetGate
    if (RecF1_F4 & mask) TargetGate = 1;
    else TargetGate = 0;
    // Set TargetActivate
    TargetActivate = 1;
    // Store new value for LastRecF1_F4
    if (TargetGate) LastRecF1_F4 |= mask; 	// set bit
    else LastRecF1_F4 &= ~mask; 		// clear bit
    return(1);
  }
  return(0);
}


unsigned char analyze_loc_7bit_message(t_message *new_dcc)
{ // {preamble} 0 0AAAAAAA 0 01DCSSSS 0 EEEEEEEE 1
  // C may be lsb of speed or headlight
  // D = direction: 1 = forward
  RecLocoAddr = (new_dcc->dcc[0] & 0b01111111);
  // see RP921 for more information
  switch (new_dcc->dcc[1] & 0b11100000)
  { case 0b00000000: break;           // 000 Decoder and Consist Control Instruction
    case 0b00100000: break;           // 001 Advanced Operation Instructions
    case 0b01000000: break;           // 010 Speed and Direction Instruction for reverse operation
    case 0b01100000: break;           // 011 Speed and Direction Instruction for forward operation
    case 0b10000000: break;           // 100 Function Group One Instruction
    case 0b10100000: break;           // 101 Function Group Two Instruction
    case 0b11000000: break;           // 110 Future Expansion
    case 0b11100000: break;           // 111 Configuration Variable Access Instruction
  }
  return(IGNORE_CMD);
}


unsigned char analyze_loc_14bit_message(t_message *new_dcc)
{ // Multi-Function (LOCO) decoders with 14 bit addresses
  // Also switches listen to these messages, as alternative means to control switch positions (via F1..F4)
  // and to respond to PoM messages. Since switches may listen to multiple decoder addresses (SkipUnEven), 
  // they also may listen to multiple LOCO addresses
  RecLocoAddr = ((new_dcc->dcc[0] & 0b00111111) << 8) | (new_dcc->dcc[1]);
  if ((RecLocoAddr >= MyFirstLocoAddr) && (RecLocoAddr <= MyLastLocoAddr)) {
    if ((new_dcc->dcc[2] & 0b11100000) == 0b10000000) {	// Function Group One (F0..F4)
      // This is a "trick: to allow setting of switches and relays via loco functions F1..F4
      // We set a TargetDevice and TargetGate if we discover a new setting of F1..F4
      // A DCC command may modify multiple functions at once. In that case we only take
      // the first. Since DCC commands for functions F1..F4 will be retransmitted, we take
      // the subsequent functions during one of the retransmissions. 
      // To avoid possible interference between loco functions F1..F4 and accessory commands,
      // we make no attempt to match the F1..F4 values with the actual "device" settings
      RecF1_F4 = (new_dcc->dcc[2] & 0b00001111);
      if (RecF1_F4 == LastRecF1_F4) return(IGNORE_CMD); // retransmission
      else {
        if (LastRecF1_F4 == 255) { // we are not yet initialised
          LastRecF1_F4 = RecF1_F4;
          return(IGNORE_CMD);
        }
        else { // something changed
          if (function_changed(0b00000001)) return(LOCO_F0F4_CMD);
          if (function_changed(0b00000010)) return(LOCO_F0F4_CMD);
          if (function_changed(0b00000100)) return(LOCO_F0F4_CMD);
          if (function_changed(0b00001000)) return(LOCO_F0F4_CMD);
          return(IGNORE_CMD); // This return statement should never be executed ...
        }
      }
    }
    if ((new_dcc->dcc[2] & 0b11100000) == 0b11100000) {	// Configuration Variable Access Instruction
      // We implement Programming of the Main (PoM), to allow changing of the feedback decoder's CV values
      // We only implement the long form of CV Access Instructions (see RP9.2.1)
      // Note that this is the only form of PoM supported by the XPressNet specification
      // {preamble} 0 10AAAAAA 0 0AAA0AA1 0 (1110CCAA 0 AAAAAAAA 0 DDDDDDDD) 0 EEEEEEEE 1
      if (!(new_dcc->dcc[2] & 0b00010000)) {			// We only support the long form
        RecCvOperation = (new_dcc->dcc[2] & 0b00001100);  	// CC bits
        RecCvOperation = RecCvOperation >> 2;
        RecCvNumber = ((new_dcc->dcc[2] & 0b00000011) << 8) | new_dcc->dcc[3];
        RecCvData = new_dcc->dcc[4];
        return(POM_CMD);
      }
    }
  }
  return(IGNORE_CMD);
}


//***************************************************************************************
// Basic Accessory (9 bit addresses) and Extended Accessory (11 bit addresses) Decoders 
//***************************************************************************************
unsigned char analyze_basic_accessory_message(t_message *new_dcc)
{ unsigned int GlobalPortAddr;  // Similar to switch address on the LH100, but starts at 0 
  unsigned char MaskedPort;	// In case of SkipUnEven, the even and uneven port are merged
  if ((new_dcc->dcc[1] >= 0b10000000) && (MyConfig == 0))
  { // BASIC ACCESSORY DECODER (with 9 bit addressing)
    // Note: this is the only form supported by the XPRESSNET specification and LENZ
    // In steps 1..4 we retrieve all necessary data from the DCC packet:
    // - RecDecAddr, RecDecPort, TargetGate and TargetActivate 
    // Step 1A: Determine the address received
    // take bits 5 4 3 2 1 0 from new_dcc->dcc[0]
    // take Bits 6 5 4 from new_dcc->dcc[1] and invert
    RecDecAddr = (new_dcc->dcc[0] & 0b00111111) | ((~new_dcc->dcc[1] & 0b01110000) << 2);
    // Step 1B: Correct the received address in case it was generated by a LENZ central station
    // In general, LENZ starts with 1, instead of 0. 
    // Further, if the received address is exactly 0, 64, 128 or 192, the address is 64 to low.
    // To compensate this, a special System CV was added.
    // Usage of this compensation is needed in case the decoder uses more than one 
    // (consecutive) address (such as the case if we skip even addresses), 
    // or provides RS-bus feedback.
    if ((my_eeprom_read_byte(&CV.CmdStation)) == 1) // Lenz system
    { if (RecDecAddr == 0) {RecDecAddr = 64;}
      else if (RecDecAddr == 64) {RecDecAddr = 128;}
      else if (RecDecAddr == 128) {RecDecAddr = 192;}
      else if (RecDecAddr == 192) {RecDecAddr = 256;}
      RecDecAddr --;
    }
    // Step 2: Determine which port (often a switch or relays) is contained within this DCC command
    // The received command suppports a range from 0..3 (thus in general 4 switches)
    // Note that the RecDecPort is NOT the same as the TargetDevice
    RecDecPort = (new_dcc->dcc[1] & 0b00000110) >> 1;
    // Step 3: Determine the gate (=coil) that is being targetted at
    TargetGate = (new_dcc->dcc[1] & 0b00000001);
    // Step 4: Determine whether this is an activate or deactivate command
    // Note that only activates may be send, and no deactivates
    if (new_dcc->dcc[1] & 0b00001000) TargetActivate = 1; else TargetActivate = 0;
    // Step 5: Determine the kind of basic accessory command. Possible options include:
    // - Normal command (may be broadcast)
    // - CV access on the main
    if (new_dcc->size == 3) // it's a normal command
    { // Note: this is the only form supported by the XPRESSNET specification and LENZ
      // Format:
      // {preamble} 0 10AAAAAA 0 1AAACDDD 0 EEEEEEEE 1
      //                AAAAAA    aaa                   = Decoder Address
      // Determine the received "global" port address (=switch address on LH100 - 1)       
      GlobalPortAddr = (RecDecAddr * 4) + RecDecPort;
      // We will calculate the TargetDevice, which may be used by the remainder of the code
      // The TargetDevice will in many cases by equivalent to the RecDecPort, except:
      // - if SkipUnEven is set 
      // - the received addrress is higher than my accessory decoder's address (= we support more addresses)
      if ((my_eeprom_read_byte(&CV.SkipUnEven)) == 1) {
        MaskedPort = ((RecDecPort & 0b00000010) >> 1);
        if (RecDecAddr >= My_Dec_Addr) {TargetDevice = (RecDecAddr - My_Dec_Addr) * 2 + MaskedPort;}
      }
      else {
        if (RecDecAddr >= My_Dec_Addr) {TargetDevice = (RecDecAddr - My_Dec_Addr) * 4 + RecDecPort;}
      }
      // Return to the calling routine the kind of command
      if (RecDecAddr == 0x01FF) {return(ACCESSORY_CMD);} // broadcast
      if ((GlobalPortAddr >= MyFirstAdrPlusCoil) && (GlobalPortAddr <= MyLastAdrPlusCoil)) return(ACCESSORY_CMD);
        else return(ANY_ACCESSORY_CMD);
    }
    else if (new_dcc->size == 6) // cv-access on the main of accessory decoder
    { // Note: this command is not supported by LENZ / Expressnet
      // Format:
      // {preamble} 10AAAAAA 0 1AAACDDD 0 (1110CCAA 0 AAAAAAAA 0 DDDDDDDD) 0 EEEEEEEE 1
      RecDecPort = new_dcc->dcc[1] & 0b00000111;     // CDDD: 1000-1111 individual output
      // 0000: all outputs
      // we ignore it.
      RecCvOperation = (new_dcc->dcc[2] & 0b00001100);  // CC bits
      RecCvOperation = RecCvOperation >> 2;        	// CC bits
      RecCvNumber = ((new_dcc->dcc[2] & 0b00000011) << 8) | new_dcc->dcc[3];
      RecCvData = new_dcc->dcc[4];
      if (RecDecAddr == My_Dec_Addr) return(POM_CMD);
    }
  }
  else if ((new_dcc->dcc[1] < 0b10000000) && (MyConfig != 0))
  { // EXTENDED ACCESSORY DECODER (with 11 bit address)
    // Note: this command is not supported by LENZ / Expressnet
    // Code could therefore not be tested.
    // take bits 2 1 from new_dcc->dcc[1]
    // take bits 5 4 3 2 1 0 from new_dcc->dcc[0]
    // take Bits 6 5 4 from new_dcc->dcc[1] and invert
    RecDecAddr = (( new_dcc->dcc[1] & 0b00000110) >> 1)
               | (( new_dcc->dcc[0] & 0b00111111) << 2)
               | ((~new_dcc->dcc[1] & 0b01110000) >> 4);
    if (new_dcc->size == 4) // it's a command
    { // Format:
      // {preamble} 0 10AAAAAA 0 0AAA0AA1 0 000XXXXX 0 EEEEEEEE 1
      // {preamble} 0 10111111 0 00000111 0 000XXXXX 0 EEEEEEEE 1
      // output mode
      RecDecPort = new_dcc->dcc[2] & 0b00011111;  // aspect
      if (RecDecAddr == 0x07FF) {return(ACCESSORY_CMD);} // broadcast
      if (RecDecAddr == My_Dec_Addr) return(ACCESSORY_CMD);
      else return(ANY_ACCESSORY_CMD);
    }
    else if (new_dcc->size == 6) // cv-access on the main of accessory decoder
    { // Note: this command is not supported by LENZ / Expressnet
      // Format:
      // {preamble} 0 10AAAAAA 0 0AAA0AA1 0 (1110CCAA 0 AAAAAAAA 0 DDDDDDDD) 0 EEEEEEEE 1      
      RecCvOperation = (new_dcc->dcc[2] & 0b00001100);// CC bits
      RecCvOperation = RecCvOperation >> 2; // CC bits      
      RecCvNumber = ((new_dcc->dcc[2] & 0b00000011) << 8) | new_dcc->dcc[3];
      RecCvData = new_dcc->dcc[4];
      if (RecDecAddr == My_Dec_Addr) return(POM_CMD);
    }
  }
  return(IGNORE_CMD);
}


//***************************************************************************************
//****** analyze_message(struct message *new_dcc) checks the received DCC message *******
//***************************************************************************************
// parameters:
//      pointer to struct of message, containing size and dcc data (supplied by the dcc_receiver)
//
// Sets CmdType: see global.h for details
// Sets various variable as "side effect": see global.h for details

void analyze_message(t_message *new_dcc)
{ unsigned char i;
  unsigned char myxor = 0;
  // Reset global variables
  CmdType = IGNORE_CMD;
  // Check if the DCC packet has a correct checksum. If error, ignore everything and return)
  for (i=0; i<new_dcc->size; i++) myxor = myxor ^ new_dcc->dcc[i];
  if (myxor)
  { // checksum error, ignore remainder of message
    DccSignalQuality ++;
    return;
  }
  // Handle the case we are in service mode (programming on the programming track)
  if (service_mode_state & (1 << SM_ENABLED)) analyze_service_mode_message(new_dcc);
  service_mode_state = 0;              // anyway  
  // We are decoding a normal DCC packet - See for steps RP 9.2.1
  if      (new_dcc->dcc[0] == 0  ) CmdType = analyze_broadcast_message(new_dcc);
  else if (new_dcc->dcc[0] <= 127) CmdType = analyze_loc_7bit_message(new_dcc);
  else if (new_dcc->dcc[0] <= 191) CmdType = analyze_basic_accessory_message(new_dcc); 
  else if (new_dcc->dcc[0] <= 231) CmdType = analyze_loc_14bit_message(new_dcc);
  else if (new_dcc->dcc[0] <= 254) {}  // Reserved in DCC for Future Use
  else {;}                             // Idle Packet
}


//***************************************************************************************
// Initialization -  must be called once at power up
//***************************************************************************************
void init_dcc_decode(void)
{ 
  DccSignalQuality = 0;		// Counter for DCC errors
  service_mode_state = 0;	// all bits off
  LastRecF1_F4 = 255;		// status of F0..F4 (= value last command)
  if ((my_eeprom_read_byte(&CV.SkipUnEven)) == 1) {
    MyFirstAdrPlusCoil = (My_Dec_Addr * 4);
    MyLastAdrPlusCoil  = (My_Dec_Addr * 4) + (NUMBER_OF_DEVICES - 1) * 2 + 1;
    MyFirstLocoAddr = My_Loco_Addr;
    MyLastLocoAddr = My_Loco_Addr + 1;
  }
  else {
    MyFirstAdrPlusCoil = My_Dec_Addr * 4;
    MyLastAdrPlusCoil  = My_Dec_Addr * 4 + NUMBER_OF_DEVICES - 1;
    MyFirstLocoAddr = My_Loco_Addr;
    MyLastLocoAddr = My_Loco_Addr;
  }
}


