//*****************************************************************************************************
//
// OpenDCC - OpenDecoder2.2
//
// Copyright (c) 2006, 2007, 2013 Kufer / Pras
//
// This source file is subject of the GNU general public license 2, that is available at
// http://www.gnu.org/licenses/gpl.txt
// 
//*****************************************************************************************************
//
// file:      cv_pom.c
// author:    Wolfgang Kufer / Aiko Pras
//
// used static: 
//   RecCvNumber
//   RecCvData 
//   RecCvOperation
//   DccSignalQuality
//
// To allow PoM, the feedback decoder listens to a loc decoder address that is equal to the 
// Decoder address + 7000.
// Transmission of PoM SET commands conforms to the NMRA standards.
// PoM VERIFY commands do NOT conform to the NMRA standards, since the CV Value is send back via 
// the RS-Bus (a proprietary solution).
// The trick to listen to a loc address is needed since LENZ equipment nor the xpressnet spcification
// supports PoM for accessory decoders.
//*****************************************************************************************************
#include <stdlib.h>
#include <stdbool.h>
#include <inttypes.h>
#include <avr/pgmspace.h>	// put var to program memory
#include <avr/io.h>
#include <avr/eeprom.h>
#include <avr/interrupt.h>

#include "global.h"		// global variables
#include "config.h"		// general definitions the decoder, cv's
#include "myeeprom.h"		// wrapper for eeprom
#include "hardware.h"		// port definitions

#include "dcc_receiver.h"	// receiver for dcc

#include "rs_bus_hardware.h"	// to check if we have an active RS-bus connection
#include "rs_bus_messages.h"	// for sending RS-bus feedback messages (after POM)
#include "led.h"                // LED specific functions



//***************************************************************************************
// Local variables
//***************************************************************************************
// The NMRA specs say we only react if a PoM message is received for the second time
// The LENZ LZV100 does always send a messages four times (thus 3 retranmission)
// The next variables are used to count the number of retransmissions.
unsigned int  PoM_CV_Current = 0;	// The CV PoM is currently targetting
unsigned char PoM_Value = 0;		// The value in the PoM command
unsigned char PoM_Prev_CV_Oper = 0; 	// The Operation PoM is currently targetting
unsigned char PoM_Attempt = 0;		// To count the number of PoM retransmissions
unsigned int  T_PoM_TimeOut = 0;	// Used to time out PoM retransmission messages


// Some CV Values should start from 0 after each decoder restart. Therefore these values
// should not be stored in EEPROM. Still we have to maintain these variables, to allow
// rerieving their value via a CV verify command.
unsigned char LocalCV23;		// Local copy of CV23 (find function: LED blinks)
unsigned char LocalCV24;		// Local copy of CV24 (PoMStart)


//***************************************************************************************
// Decoder specific part / should be changed for different hardware
//***************************************************************************************
// Only for some CVs new values should be stored in EEPROM
// For an overview of access rights, see comments in cv_define.h
// Only the following CV ranges should be saved in EEPROM:
// - CV1       (Accessory address - low)
// - CV3-CV6   (Time on F1 .. Time on F4)
// - CV9       (Accessory address - high)
// - CV10      (RS-Bus address)
// - CV19-CV21 (CmdStation, RSRetry, SkipUnEven)
// - CV27      (DecType) => only in case future extension boards get supported
// - CV33-CV34 (SendFB, AlwaysAct)

unsigned char save_cv_value_in_EEPROM(unsigned int cv)
{ unsigned int cvNumber = cv + 1; // cv starts with 0
  if  (cvNumber == 1) return(1);
  if ((cvNumber >= 3) && (cvNumber <= 6)) return(1);
  if  (cvNumber == 9) return(1);
  if  (cvNumber == 10) return(1);
  if ((cvNumber >= 19) && (cvNumber <= 21)) return(1);
  /* In case we accomodate extension boards, CV27 may become "settable"
  if  (cvNumber == 27) return(1); */
  if ((cvNumber >= 33) && (cvNumber <= 34)) return(1);
  return(0);
}


//***************************************************************************************
// Restore all eeprom content to default and reboot
//***************************************************************************************
void ResetDecoder(void)
{ unsigned int i;
  unsigned char default_value;
  unsigned char *eeptr;
  const unsigned char *pgmptr;
  unsigned char blink = 16;
  LED_ON;
  eeptr = (unsigned char *) &CV;
  pgmptr = (const unsigned char *) &CV_PRESET;
  for (i=0; i < sizeof(CV); i++)
  { // restore EEPROM only if value has been modified
    default_value = pgm_read_byte(pgmptr);
    // Change everything, including all decoder addresses
    if ((my_eeprom_read_byte(eeptr) != default_value))
    { my_eeprom_write_byte(eeptr, default_value);
      blink--;
      if (blink == 8) LED_OFF;
      if (blink == 0) {LED_ON; blink = 16;}
    }
    eeptr++;
    pgmptr++;
  }
  eeprom_busy_wait();
  LED_OFF;
}


//***************************************************************************************
// CV Verify code
//***************************************************************************************
void cv_verify_sm(void)
{ // For Service Mode programming we implement verify command according to NMRA specs.
  if (my_eeprom_read_byte(&CV.myAddrL + RecCvNumber) == RecCvData) activate_ACK(6);
}

void cv_verify_pom(void)
{ // According to the NMRA the verify command checks if there is a match
  // between the value in the PoM command and the value stored in the decoder.
  // Such behavior is useful for Service Mode Programming, but not for PoM.
  // Since we can send information back via the RS-bus, we modify this behavior
  // and send the value stored in the decoder back.
  // Note that all CV values can be retrieved from EEPROM, except:
  // - CV23 (find function which blinks led)
  // - CV24 (PoM Start)
  // - CV26 (DccQuality)
  if      (RecCvNumber == (23-1)) send_CV_value_via_RSbus(LocalCV23);
  else if (RecCvNumber == (24-1)) send_CV_value_via_RSbus(LocalCV24);
  else if (RecCvNumber == (26-1)) send_CV_value_via_RSbus(DccSignalQuality);
  else send_CV_value_via_RSbus(my_eeprom_read_byte(&CV.myAddrL + RecCvNumber));
}


//***************************************************************************************
// CV Bit operation code
//***************************************************************************************
void cv_bitoperation_sm(void)
{ // Data is interpreted as 111KDBBB
  // K = 0 verify, K = 1 write
  // D = value
  // BBB = Bitpos.
  unsigned char bitmask;
  unsigned char oldbyte;
  bitmask = 1 << (RecCvData & 0b00000111);
  if (RecCvData & 0b00010000)
  { // write bit
    if (save_cv_value_in_EEPROM (RecCvNumber)) {
      oldbyte = my_eeprom_read_byte(&CV.myAddrL + RecCvNumber);
      if (RecCvData & 0b00001000) oldbyte |= bitmask;
      else                        oldbyte &= ~bitmask;
      my_eeprom_write_byte(&CV.myAddrL + RecCvNumber, oldbyte);
      eeprom_busy_wait();
      activate_ACK(6);
    }
  }
  else
  { // verify bit
    if (RecCvData & 0b00001000)
    { if (my_eeprom_read_byte(&CV.myAddrL + RecCvNumber) & bitmask) activate_ACK(6); }
    else
    { if ((my_eeprom_read_byte(&CV.myAddrL + RecCvNumber) & bitmask) == 0) activate_ACK(6);}
  }
}

//***************************************************************************************
// Main function
//***************************************************************************************
void cv_operation(unsigned char op_mode)
{  // Ensure we only react on the second transmission of the same PoM message
  if ((PoM_CV_Current == RecCvNumber) && (PoM_Value == RecCvData) && (PoM_Prev_CV_Oper == RecCvOperation))
  {
    PoM_Attempt ++;		// Count the number of retransmissions
  }
  else {
    PoM_Attempt = 1;
    PoM_CV_Current = RecCvNumber;
    PoM_Value = RecCvData;
    PoM_Prev_CV_Oper = RecCvOperation;
  }
  if (PoM_Attempt != 2) return;
  // If we're here we received the same PoM message for the second time.
  // CV Remapping: CV513 = CV1
  RecCvNumber &= 0x1FF;
  // Stop processing if we don't have a valid CV address
  // Thus *protect other memory from (accidentally) getting overwritten.
  // Note addresses on the wire start with 0, whereas counting starts with 1
  if (RecCvNumber > (sizeof(CV) - 1)) return;
  switch(RecCvOperation) {
    case CV_NOP: break;
    case CV_VERIFY:
      if (op_mode == SM_CMD) cv_verify_sm();
      else cv_verify_pom();
      break;
    case CV_WRITE:
      // Reset decoder data to initial values if we'll write to CV8 (coded as 7) the value 0x0D 
      if ((RecCvNumber == (8-1)) && (RecCvData == 0x0D)) {
        if (op_mode == SM_CMD) activate_ACK(6);
        ResetDecoder();
        _restart();                         // really hard exit
      }
      // Restart the decoder but do not reset the EEPROM data (CVs)
      // Use this function after PoM has changed CV values and new values should take effect now
      if ((RecCvNumber == (25-1)) && (RecCvData)) { 
        _restart();                         // really hard exit
      }
      // Search function: blink the decoder's LED if CV23 is set to 1. 
      // Continue blinking until CV23 is set to 0
      if (RecCvNumber == (23-1)) { 
        if (RecCvData) {LocalCV23 = 1; flash_led_fast(8);}
        else {LocalCV23 = 0; turn_led_off();}
        break;
      }
      // Check if the value of the received CV should be saved in EEPROM
      if (save_cv_value_in_EEPROM (RecCvNumber)) {
        my_eeprom_write_byte(&CV.myAddrL + RecCvNumber, RecCvData);
        eeprom_busy_wait();
        if (op_mode == SM_CMD) {activate_ACK(6); _restart();}
      }
      break;
    case CV_BITOPERATION: 
      // CV Bit Operation is only implemented for Service Mode
      if (op_mode == SM_CMD) cv_bitoperation_sm();
      break;
  } 
}


//***************************************************************************************
// Time out to allow processing of the same CV after 2 seconds has passed
//***************************************************************************************
void check_PoM_time_out(void) { 
  // this function is called from main every 20 seconds
  // maintain a counter, to time-out after 2 seconds
  T_PoM_TimeOut ++;
  if (T_PoM_TimeOut > 100) {		// 2s have passed since previous PoM message
    PoM_Attempt = 0;			// Forget previous POM messages
    T_PoM_TimeOut = 0;			// Reintialise timeout (is strictly speaking not needed)
  }
}


