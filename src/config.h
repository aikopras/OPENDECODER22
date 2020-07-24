//------------------------------------------------------------------------
//
// OpenDCC - OpenDecoder2
//
// Copyright (c) 2007 Kufer
//
// This source file is subject of the GNU general public license 2,
// that is available at the world-wide-web at
// http://www.gnu.org/licenses/gpl.txt
// 
//------------------------------------------------------------------------
//
// file:      config.h
// author:    Wolfgang Kufer
// contact:   kufer@gmx.de
// webpage:   http://www.opendcc.de
// history:   2007-02-14 V0.1  kw start
//            2011-12-31 V0.14 ap changed #define OPENDECODER22 0x2F
//
//------------------------------------------------------------------------
//
// purpose:   flexible general purpose decoder for dcc
//            This is the central definition file for the project
//
//------------------------------------------------------------------------
//
// content:   1. Project settings
//               1.a) General
//               1.b) Configuration of Software Modules
//
//            2. EEPROM definitions (CV's)
//            3. Global variables
//            4. Useful inline routines (delay, Semaphor-operations)
//
//========================================================================
//
#ifndef _CONFIG_H_
#define _CONFIG_H_

//========================================================================
// 1. Project Definitions
//========================================================================
//
// 1.a) General
//
// #define OPENDECODER_VERSION   0x2F

// Definition of target hardware:
//   
#define OPENDECODER22 0x22
//  OPENDECODER22: this board has:
//                 - ATmega8535, 16A, 32A, 164, 324 or 644 @ 11059200 Hz 
//                 - 8 Powerports with integrated feedback (for 4 switches or relais)
//                 - RS-bus feedback
//                 Note: port and pins are NOT compatible with other decoder 2 hardware
//




//-------------------------------------------------------------------------------------------
// Decoder Model Configuration Check

#if (TARGET_HARDWARE != OPENDECODER22)
  #warning: This code will only run with OPENDECODER22
#endif


//========================================================================
// 2. EEPROM Definitions (CV's)
//========================================================================
//
// content is defined in config.c

#include "cv_define.h"

extern t_cv_record CV EEMEM;

extern const t_cv_record CV_PRESET PROGMEM;



//========================================================================
// 3. Global variables
//========================================================================

//---------------------------------------------------------------------
// Timing Definitions:
//

#define TICK_PERIOD 20000L       // 20ms tick for Timing Engine
                                 // => possible values for timings up to
                                 //    5.1s (=255/0.020)
                                 // note: this is also used as frame for
                                 // Servo-Outputs (OC1A and OC1B)

//----------------------------------------------------------------------------

extern volatile signed char timerval;     // gets incremented in the timetick

// The following flag is included to move non-critical code away from the timer1 ISR
// Main is now checking this flag
extern volatile unsigned char timer1fired; // Indicates timer 1 has fired



//========================================================================
// 4. Project Definitions
//========================================================================
//

//-------------------------------------------------------------------------
// 4.a) Definitions for inter process communication
//-------------------------------------------------------------------------
// These routines are *inline*, so we keep them in the header.
//
// usage:   semaphor_set(flag, state) to do the communication.
//          semaphor_get(flag)




extern volatile unsigned char Communicate;


#define C_Received        0    // a new DCC was received - issued by ISR(Timer1)
                               //                          cleared by main
#define C_DoSave          1    // a new PORT state should be saved
                               //                        - issued by action
                               //                          cleared by main
#define C_Tick            2    // a Tickevent happened
  

//========================================================================
// 4.b) Useful inline code
//========================================================================

static inline unsigned char semaphor_query(unsigned char flag) 
       __attribute__((always_inline));

unsigned char semaphor_query(unsigned char flag)
  {
    return (Communicate & (1<<flag));
  }
               
static inline void semaphor_set(unsigned char flag) 
       __attribute__((always_inline));

void semaphor_set(unsigned char flag)
  {
    cli();
    Communicate |= (1<<flag);
    sei();
  }
       
static inline unsigned char semaphor_get(unsigned char flag) 
       __attribute__((always_inline));

unsigned char semaphor_get(unsigned char flag)
  {
    unsigned char value;
    cli();
    value = Communicate & (1<<flag);
    Communicate &= (~(1<<flag));
    sei();
    return(value);
  }
        

//------------------------------------------------------------------------
// Delay-Macro (all values in us) -> this is busy waiting
//------------------------------------------------------------------------
//
// same macro as in util/delay.h, but use up to some ms.
// The maximal possible delay is 262.14 ms / F_CPU in MHz.
// This is 16ms for 16MHz; longest used delay: 1000us

#ifndef _UTIL_DELAY_H_
  #include <util/delay.h>
#endif

static inline void _mydelay_us(double __us) __attribute__((always_inline));
void
_mydelay_us(double __us)
{
    uint16_t __ticks;
    double __tmp = ((F_CPU) / 4e6) * __us;
    if (__tmp < 1.0)
        __ticks = 1;
    else if (__tmp > 65535)
        __ticks = 0;    /* i.e. 65536 */
    else
        __ticks = (uint16_t)__tmp;
    _delay_loop_2(__ticks);
}   

static inline void _restart(void) __attribute__((always_inline));
void
_restart(void)
{
    cli();
                    
    // laut diversen Internetseiten sollte folgender Code laufen -
    // tuts aber nicht, wenn man das Assemblerlistung ansieht.
    // void (*funcptr)( void ) = 0x0000;    // Set up function pointer
    // funcptr();                        // Jump to Reset vector 0x0000
    
    __asm__ __volatile 
    (
       "ldi r30,0"  "\n\t"
       "ldi r31,0"  "\n\t"
       "icall" "\n\t"
     );
}


#endif   // _config_h_
