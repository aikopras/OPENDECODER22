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
// file:      switch.c
// author:    Aiko Pras
// history:   2012-01-08 V0.1 ap based upon port_engine.c from the OpenDecoder2 project => relays.c
//            2013-12-25 V0.2 ap based upon relays.c => switch.c
//				 changed all relay specific code in switch specific code
//            2015-01-06 V0.3 ap Changed switch numbering such that it is now left to right
//
//
// A DCC Switch Decoder for ATmega16A and other AVR.
//
// Uses the following global variables:
// - TargetDevice: the switch / relays4 that is being addressed. Range: 0 .. (NUMBER_OF_DEVICES - 1)
//   For normal switch / relays4 decoders, the range will be 0..3
// - TargetGate: Targetted coil within that Port. Usually - or + / green or red
// - TargetActivate: Coil activation (value = 1) or deactivation (value = 0) 
//
//*****************************************************************************************************

#include <stdlib.h>
#include <stdbool.h>
#include <inttypes.h>
#include <avr/pgmspace.h>
#include <avr/io.h>
#include <avr/eeprom.h>
#include <avr/interrupt.h>

#include "global.h"
#include "config.h"
#include "myeeprom.h"
#include "hardware.h"
#include "timer1.h"
#include "switch.h"
#include "led.h"

//*****************************************************************************************************
//************************************ Definitions and declarations ***********************************
//*****************************************************************************************************
#define GREEN 	          0         // The green coil 
#define RED 	          1         // The red coil 
#define UNKNOWN           2         // If we start up and do not know the position before power-down


typedef struct {
  unsigned char gate_pos;	    // which of the two gates is currently on (RED or GREEN)
  unsigned char hold_time;	    // maximum pulse duration to activate the gate (in 20 ms ticks)
  unsigned char rest_time;	    // remaining puls duration during gate activation
} t_device;

t_device devices[4];		    // we have 4 devices (switches, relays, ...) with each two coils  

unsigned char always_activate_coil; // Coil will be activated even if it is already in the right position

//*****************************************************************************************************
//********************************** Local functions (called locally) *********************************
//*****************************************************************************************************
void init_switch(unsigned char device) {
  // device range: 0..3
  // initialise the "administration" for the requested switch
  // Although we could measure the gate position, not doing so has as only "disadvantage" that the
  // first time the gate (switch) is activated, it may allready have been in the requested position. 
  devices[device].gate_pos = UNKNOWN;
  // store the maximum puls time
  devices[device].hold_time = my_eeprom_read_byte(&CV.T_on_F1 + device);
  // in case of relays, initialise the gate to a default position by setting the remaining puls time
  if (MyType == TYPE_RELAYS4) {
    devices[device].rest_time = 0;
  }
}

// The routine below is (currently) not being used. It would set switches / relays to a predefined
// (=GREEN) position, and block for roughly 50 ms, to avoid that multiple switches will change together.
void init_relays_and_block(unsigned char device) {
  // Similar to above, but this time set the relay to a defined position and subsequently 
  // block for roughly 50 ms
  // device range: 0..3
  // initialise the "administration" for the requested relay
  devices[device].gate_pos = GREEN;
  // store the maximum puls time
  devices[device].hold_time = my_eeprom_read_byte(&CV.T_on_F1 + device);
  // initialise the gate to a default position by setting the remaining puls time
  // never do this for switches, however...
  devices[device].rest_time = devices[device].hold_time;
  // do the actual setting of the coil now! Note we set the first (of both) coils
  unsigned char coil = device * 2;	
  OUTPUT_PORT |= (0x80>>coil);	// activate the first gate (coil) of the relays
  // Wait for roughly 50 seconds now
  unsigned char number_of_ticks = 0;
  while (number_of_ticks < 3) {
    if (timer1fired) { 
      number_of_ticks ++;
      check_led_time_out();
      check_switch_time_out();
    }
    timer1fired = 0;
  }
}

//*****************************************************************************************************
//********************************* Main functions (called externally) ********************************
//*****************************************************************************************************
void init_switches(void) {
  // first disable all outputs
  OUTPUT_PORT = 0x00;
  // initialise for all switches the "administration"
  // continue from the last switches positions before power-down.
  init_switch(0);
  init_switch(1);
  init_switch(2);
  init_switch(3);
  // note that we should call init_relay_and_block() in case we want to set the 
  // relays to a predefined position and wait 50 ms between setting each relay  
  // 
  // Determine of coils should only be activated in case their position has changed, or if they 
  // should allways be activated after reception of a Accesory Decoder command. In the later case 
  // the coil will be activated again for each retranmission, or in case of a handheld as long as
  // the + or - button is pushed.
  // Note that, in case of AlwaysAct, a Feedback message will be generated even in case the switch 
  // has not changed. Some programs, such as Railware, expect such behavior.
  always_activate_coil = my_eeprom_read_byte(&CV.AlwaysAct);
}


void set_switch(void) { 
  // This function is called from main, after a DCC accessory decoder command  
  // or a loco F1..F4 command is received
  // We do timer-based de-activation, so no need to react on de-activation messages 
  if (TargetActivate) {
    // Always react, even if the current gate position is the same as the requested position
    // This ensures that the coil will always be activated, and a feedback message being send
    // As a consequence, the coil may receive many pulses in a row
    if ((devices[TargetDevice].gate_pos != TargetGate) || (always_activate_coil != 0)) { 
      activity_led();
      // Note: Switch and Relays PCBs connect the output port in different ways
      if (MyType == TYPE_SWITCH) {
        // For switch decoders, TargetDevice is in the range 0..3
        // first deactivate all gates (coils)
        OUTPUT_PORT &= ~(0x80>>(2*TargetDevice)); 		// clear first gate (coil) of this device
        OUTPUT_PORT &= ~(0x80>>(2*TargetDevice + 1));		// clear second gate (coil) of this device
        // select the active gate
        devices[TargetDevice].gate_pos = TargetGate;
        // set the activation time
        devices[TargetDevice].rest_time = devices[TargetDevice].hold_time;
        // Activate the gate (coil)
        OUTPUT_PORT |= (0x80>>(2*TargetDevice + TargetGate));	// set the requested port
      }
      if (MyType == TYPE_RELAYS4) {
        // Also for Relays-4 decoders, TargetDevice is in the range 0..3
        // first deactivate all gates (coils)
        OUTPUT_PORT &= ~(1<<(2*TargetDevice));			// clear first gate (coil) of this device
        OUTPUT_PORT &= ~(1<<(2*TargetDevice + 1));		// clear second gate (coil) of this device
        // select the active gate
        devices[TargetDevice].gate_pos = TargetGate;
        // set the activation time
        devices[TargetDevice].rest_time = devices[TargetDevice].hold_time;
        // Activate the gate (coil)
        OUTPUT_PORT |= (1<<(2*TargetDevice + 1 - TargetGate));	// set the requested port
      }
    }
  }
} 


void check_switch_time_out(void) { 
  // This function is called from main, every time tick (20 ms)  
  unsigned char i;
  unsigned char rest_ticks;
  for (i=0; i<4; i++) {				// check each device (relays, switch, ...)
    rest_ticks = devices[i].rest_time;		// use a local variable to force compiler to tiny code
    if (rest_ticks !=0) {			// coil is active / active time is not over yet
      rest_ticks = rest_ticks - 1;		// decrease remaining time coil should still be active
      if (rest_ticks == 0) {			// coil should no longer be active?
        if (MyType == TYPE_SWITCH) {
          OUTPUT_PORT &= ~(0x80>>(2*i));	// clear first gate (coil) of this device
          OUTPUT_PORT &= ~(0x80>>(2*i + 1));	// clear second gate (coil) of this device
        }
        if (MyType == TYPE_RELAYS4) {
          OUTPUT_PORT &= ~(1<<(2*i));		// clear first gate (coil) of this device
          OUTPUT_PORT &= ~(1<<(2*i + 1));	// clear second gate (coil) of this device
        }
      }
      devices[i].rest_time = rest_ticks;
    }
  }
}

