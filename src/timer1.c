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
// file:      port_engine.c
// author:    Wolfgang Kufer
// history:   2007-02-20 V0.1 kw copied from opendecoder.c
//                               removed code for versatile output -
//                               we only do turnout commands and permanent
//                               commands (up to now)
//            2011-12-31 V0.2 ap Removed everything, except the timer related code
//
//------------------------------------------------------------------------
//
// purpose:   flexible general purpose decoder for dcc
//            here: timing engine for local led flashing
//
//             1. Defines and variable definitions
//             2. Init
//             3. ISR
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
#include "myeeprom.h"
#include "hardware.h"
#include "dcc_receiver.h"
#include "main.h"
#include "timer1.h"


//*****************************************************************************************************
//********************************************* Defines ***********************************************
//*****************************************************************************************************
#define TICK_PERIOD 20000L       // 20ms tick for Timing Engine
                                 // => possible values for timings up to
                                 //    5.1s (=255/0.020)


// Timer 1 specific settings
#if defined ENHANCED_PROCESSOR
#define TC1_Interrupt_Mask_Register			TIMSK1		// Register
#define TC1_Input_Capture_Interrupt_Enable		ICIE1		// Bit definition
#else 
#define TC1_Interrupt_Mask_Register			TIMSK
#define TC1_Input_Capture_Interrupt_Enable		TICIE1		// Bit definition
#define TC1_Output_Compare_Match_Interrupt_Enable	OCIE0 
#endif

//*****************************************************************************************************
//*************************************** Initialise Timer 1 ******************************************
//*****************************************************************************************************
// initializes the timer (with TICK_PERIOD)

void init_timer1(void)
  {
    // Init Timer1 as Fast PWM with a CLKDIV (prescaler) of 8
    #define T1_PRESCALER   8    // may be 1, 8, 64, 256, 1024
    #if   (T1_PRESCALER==1)
        #define T1_PRESCALER_BITS   ((0<<CS12)|(0<<CS11)|(1<<CS10))
    #elif (T1_PRESCALER==8)
        #define T1_PRESCALER_BITS   ((0<<CS12)|(1<<CS11)|(0<<CS10))
    #elif (T1_PRESCALER==64)
        #define T1_PRESCALER_BITS   ((0<<CS12)|(1<<CS11)|(1<<CS10))
    #elif (T1_PRESCALER==256)
        #define T1_PRESCALER_BITS   ((1<<CS12)|(0<<CS11)|(0<<CS10))
    #elif (T1_PRESCALER==1024)
        #define T1_PRESCALER_BITS   ((1<<CS12)|(0<<CS11)|(1<<CS10))
    #endif

    // check TICK_PERIOD and F_CPU
    #if (F_CPU / 1000000L * TICK_PERIOD / T1_PRESCALER) > 65535L
      #warning: overflow in ICR1 - check TICK_PERIOD and F_CPU
      #warning: suggestion: use a larger T1_PRESCALER
    #endif    
    #if (F_CPU / 1000000L * TICK_PERIOD / T1_PRESCALER) < 5000L
      #warning: resolution accuracy in ICR1 too low - check TICK_PERIOD and F_CPU
      #warning: suggestion: use a smaller T1_PRESCALER
    #endif    

    // Timer 1 runs in FAST-PWM-Mode with ICR1 as TOP-Value (WGM13:0 = 14).
    // note: due to a bug in AVRstudio this can't be simulated !!

    ICR1 = F_CPU / 1000000L * TICK_PERIOD / T1_PRESCALER ;  

    OCR1A = F_CPU / 1000000L * TICK_PERIOD / T1_PRESCALER / 20;  // removed 24.12.2008 ???
    OCR1B = F_CPU / 1000000L * TICK_PERIOD / T1_PRESCALER / 15;   

    // OC1A and OC1B are mapped to Timer (for Servo Operation)
    //   (1 << COM1A1)          // compare match A
    // | (0 << COM1A0)          // Clear OC1A/OC1B on Compare Match, set OC1A/OC1B at TOP
    // | (1 << COM1B1)          // compare match B
    // | (0 << COM1B0) 

    TCCR1A = (0 << COM1A1)          // compare match A
           | (0 << COM1A0)          // not activated yet -> this is done in init_servo();
           | (0 << COM1B1)          // compare match B
           | (0 << COM1B0) 
           | 0                      // reserved
           | 0                      // reserved
           | (1 << WGM11)  
           | (0 << WGM10);          // Timer1 Mode 14 = FastPWM - Int on Top (ICR1)
    TCCR1B = (0 << ICNC1) 
           | (0 << ICES1) 
           | (1 << WGM13) 
           | (1 << WGM12) 
           | (T1_PRESCALER_BITS);   // clkdiv

    TC1_Interrupt_Mask_Register |= (1<<TOIE1)             // Timer1 Overflow
           | (0<<OCIE1A)            // Timer1 Compare A
           | (0<<OCIE1B)            // Timer1 Compare B
           | 0                      // reserved
           | (0<<TC1_Input_Capture_Interrupt_Enable);     // Timer1 Input Capture
      

    timerval = 0;          
  }


//*****************************************************************************************************
//*********************************** TIMER ISR with dedicated code ***********************************
//*****************************************************************************************************
// Declare both functions as "always_inline", so the compiler will include code into the calling isr
// In this way we avoid the (expensive) overhead of calling the functions. 
static inline void disable_timer_interrupt(void)   __attribute__((always_inline));
void disable_timer_interrupt(void)
{
  TC1_Interrupt_Mask_Register &= ~(1<<TOIE1);        // Timer1 Overflow
}

static inline void enable_timer_interrupt(void)   __attribute__((always_inline));
void enable_timer_interrupt(void) 
{
  TC1_Interrupt_Mask_Register |= (1<<TOIE1);        // Timer1 Overflow
}

  
// Timer1 (with prescaler 8 and 16 bit total count) triggers an interrupt
// every TICK_PERIOD (=20ms @8MHz)
ISR(TIMER1_OVF_vect) {                    // Timer1 Overflow Int
  disable_timer_interrupt();
  sei();                                  // allow DCC interrupt
  timerval++;                             // advance global clock
  timer1fired = 1;
  enable_timer_interrupt();
}


//*****************************************************************************************************
//********************* Timer related functions used by handle_occupied_tracks() **********************
//*****************************************************************************************************
// Next are some timers related to sending over the RS-Bus
unsigned char Startup_Delay = 0;        // Before we send the first RS-bus message, we'll wait 200 msec
unsigned char Feedback_delay = 0;	// Interval in (20 ms ticks) between RS-bus transmission attempts


// The RS-bus can send a maximum of 1 packet every 30 ms.  
// Therefore it is useless to create RS-bus messages at a higher rate. 
// This function is called by handle_occupied_tracks(). 
// Since handle_occupied_tracks() is called every 20 ms, this function is called every 20 ms
unsigned char time_for_next_feedback(void) {
  Feedback_delay ++;
  if (Feedback_delay > 2) {	// around 40 ms have passed since last time
    Feedback_delay = 0;
    return 1;
  }
  return 0;
}


// After startup we wait 200 msec before sending the first RS-Bus message
// This allows all inputs to become stable
// This function is called every 40 ms by handle_occupied_tracks()
unsigned char start_up_phase(void) {
  if (Startup_Delay > 5) {
    Startup_Delay = 255;
    return (0);
  }
  else {
    Startup_Delay++;
    return (1);
  }
}



