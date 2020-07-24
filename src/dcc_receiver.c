//------------------------------------------------------------------------
//
// OpenDCC - OpenDecoder2
//
// Copyright (c) 2006, 2007 Kufer
//
// This source file is subject of the GNU general public license 2,
// that is available at the world-wide-web at
// http://www.gnu.org/licenses/gpl.txt
// 
//------------------------------------------------------------------------
//
// file:      dcc_receiver.c
// author:    Wolfgang Kufer
// contact:   kufer@gmx.de
// webpage:   http://www.opendcc.de
// history:   2007-02-14 V0.1 kw copied from opendecoder.c
//            2007-02-26 V0.2 kw added config.h
//            2007-03-01 V0.3 kw added code for sampling receiver with
//                               lowpass filter
//                               (not yet tested)
//            2007-03-30 V0.4 kw added local memory for message, copy to
//                               global messagebuffer at end of incoming
//                               dcc-message -> this releases timing!
//                               T87us now T77us
//            2007-05-21 V0.5 kw added some comments
//            2007-02-07 V0.6 kw changed preamble detection limit 
//                               from 11 to 10 'one' bits
//            2010-11-28 V0.7 ap make code ready for OPENDECODER22 and 
//                               processors like ATMega164, 324 and 644
//                               Note that NO functional changes were made.
//                               In the beginning of the file many aliases were added to make the
//                               code less hardware specific and also capable to run on the newly
//                               designed PCB for hardware 2.2.
//                               Since AVRs 164, 324 & 644 use timers in a different way, the
//                               pre-compiler directive "ENHANCED_PROCESSOR" is used for
//                               conditional code segments
//            2011-12-31 V0.8 ap Change code for OPENDECODER22GBM
//                               Removed some generic OPENDECODER code not being used
//            2013-02-24 V0.9 ap Rewrote some parts to make this file applicable for all versions
//				 of Opendecoder V2.2 (GBM specific parts "isolated" in #if statements)
//
//------------------------------------------------------------------------
//
// purpose:   flexible general purpose decoder for dcc
//            here: the dcc receive parts
//
// content:   A DCC-Decoder for ATmega8515 and other AVRs
//
//             1. Defines and variable definitions
//             2. DCC receive routine
//             3. Test and Simulation
//
// note:      for railcom a precise detection of the packet end is
//            required to find out the cutout point!
//            this requires either:
//            a) using the old isr and correct wiring
//            b) usinge the new isr and detecting both sides of DCC
//------------------------------------------------------------------------
// used hw resources:
//
//      INT0:   DCCIN (note: for original 8535 based AVR boards INT1 is used)
//      Timer0: for T77us Delay 
//      Overflow Interrupt Timer0: (evaluating DCCIN Level)
//      DCC_ACK (for acknowledge)

#include <stdlib.h>
#include <stdbool.h>
#include <inttypes.h>
#include <avr/pgmspace.h>        // put var to program memory
#include <avr/io.h>
#include <avr/eeprom.h>
#include <avr/interrupt.h>
#include <string.h>

#include "config.h"
#include "hardware.h"            // Port and CPU definitions
#include "dcc_receiver.h"


//---------------------------------------------------------------------------
// Define all hardware specific settings at the beginning
// As opposed to the original OpenDecoder hardware, we must use INT1

#if (TARGET_HARDWARE == OPENDECODER22) || (TARGET_HARDWARE == OPENDECODER22GBM) 
  #define DCC_Interrupt_Vector					INT1_vect	// We use Interrupt 1 
  #define DCC_Interrupt_Port					INT1
  #define DCC_Interrupt_Sense_Control_Bit_0			ISC10		// Bit setting
  #define DCC_Interrupt_Sense_Control_Bit_1			ISC11		// Bit setting
#else
  #define DCC_Interrupt_Vector					INT0_vect	// We use Interrupt 0
  #define DCC_Interrupt_Port					INT0
  #define DCC_Interrupt_Sense_Control_Bit_0			ISC00		// Bit setting
  #define DCC_Interrupt_Sense_Control_Bit_1			ISC01		// Bit setting
#endif


// Interrupt specific settings
#if defined ENHANCED_PROCESSOR
  #define Interrupt_Select_Register				EIMSK    	// External Interrupt Mask Register
  #define Interrupt_Control_Register				EICRA   	// External Interrupt Control Register
#else 
  #define Interrupt_Select_Register				GICR    	// General Interrupt Control Register
  #define Interrupt_Control_Register				MCUCR   	// MCU Control Register
#endif

// Timer 0 specific settings
#if defined ENHANCED_PROCESSOR
  #define TC0_Interrupt_Mask_Register				TIMSK0		// Register
  #define TC0_Control_Register_A				TCCR0A		// Register
  #define TC0_Control_Register_B				TCCR0B		// Register
  #define TC0_Force_Output_Compare 				FOC0A		// Bit definition
  #define TC0_Compare_Match_Output_0				COM0A0		// Bit definition
  #define TC0_Compare_Match_Output_1				COM0A1		// Bit definition
#else 
  #define TC0_Interrupt_Mask_Register				TIMSK
  #define TC0_Control_Register_A				TCCR0		// Note: A and B are
  #define TC0_Control_Register_B				TCCR0		// here the same register
  #define TC0_Force_Output_Compare 				FOC0
  #define TC0_Compare_Match_Output_0				COM00
  #define TC0_Compare_Match_Output_1				COM01
#endif

//---------------------------------------------------------------------------
// 
// Note: ACK is done with busy waiting - will be stretched when interrupts occur.

void activate_ACK(unsigned char time)
  {
    unsigned char i;
    // set ACK for  time [ms]
    DCC_ACK_ON;
    for (i=0; i<time; i++) _mydelay_us(1000);
    DCC_ACK_OFF;    
  }


/// This are DCC timing definitions from NMRA
#define PERIOD_1   116L          // 116us for DCC 1 pulse - do not change
#define PERIOD_0   232L          // 232us for DCC 0 pulse - do not change



unsigned char Recstate;         


void init_dcc_receiver(void)
  {

    // Init Timer0
    // Determine optimal value for PRESCALER
    #define T0_PRESCALER   8    // may be 1, 8, 64, 256, 1024
    #if   (T0_PRESCALER==1)
        #define T0_PRESCALER_BITS   ((0<<CS02)|(0<<CS01)|(1<<CS00))
    #elif (T0_PRESCALER==8)
        #define T0_PRESCALER_BITS   ((0<<CS02)|(1<<CS01)|(0<<CS00))
    #elif (T0_PRESCALER==64)
        #define T0_PRESCALER_BITS   ((0<<CS02)|(1<<CS01)|(1<<CS00))
    #elif (T0_PRESCALER==256)
        #define T0_PRESCALER_BITS   ((1<<CS02)|(0<<CS01)|(0<<CS00))
    #elif (T0_PRESCALER==1024)
        #define T0_PRESCALER_BITS   ((1<<CS02)|(0<<CS01)|(1<<CS00))
    #endif
    // Define 77 microseconds
    #define T77US (F_CPU * 77L / T0_PRESCALER / 1000000L)
    // Check if prescaler is optimal for this Xtal
    #if (T77US > 254)
      #warning T77US too big, use either larger prescaler or slower processor
    #endif
    #if (T77US < 32)
      #warning T77US too small, use either smaller prescaler or faster processor
    #endif


    TC0_Control_Register_A |= (0 << WGM00)			// Timer0: Normal mode
                           |  (0 << WGM01)
                           |  (0 << TC0_Compare_Match_Output_0)
                           |  (0 << TC0_Compare_Match_Output_1);
    TC0_Control_Register_B |= (0 << TC0_Force_Output_Compare)	 
                           |  (0 << CS02)			// cs02.01.00 : 0  0  0 = Timer0: stopped
                           |  (0 << CS01)			//            : 0  0  1 = run 1:1
                           |  (0 << CS00);			//            : 0  1  0 = run with prescaler 8

    TCNT0 = 256L - T77US;  
    // OCR0 is unused -> Flags!

    semaphor_get(C_Received);

    TC0_Interrupt_Mask_Register |= (1<<TOIE0);       // Timer0 Overflow
    // End of init Timer0
    
    // Init Interrupt for DCC Port (INT1)
    Interrupt_Select_Register |= (1<<DCC_Interrupt_Port);
    // For correct detection of the DCC packets, we have to trigger on the risinging edge of the input (J) signal
    Interrupt_Control_Register |= (1<<DCC_Interrupt_Sense_Control_Bit_1)  // The rising edge of the signal 
                               |  (1<<DCC_Interrupt_Sense_Control_Bit_0); // generates an interrupt request.
  }



//==============================================================================
//
// Section 2
//
// DCC Receive Routine
//
// Howto:    uses two interrupt: a rising edge in DCC polarity triggers INT0;
//           in INT0, Timer0 with a delay of 77us is started.
//           On Timer0 Overflow Match the level of DCC is evaluated and
//           parsed.
//
//                           |<-----116us----->|
//
//           DCC 1: _________XXXXXXXXX_________XXXXXXXXX_________
//                           ^-INT0
//                           |----77us--->|
//                                        ^Timer-INT: reads zero
//
//           DCC 0: _________XXXXXXXXXXXXXXXXXX__________________
//                           ^-INT0
//                           |----------->|
//                                        ^Timer-INT: reads one
//           
// Result:   1. The received message is collected in the struct "local"
//           2. After receiving a complete message, data is copied to
//              "incoming".
//           3. The flag C_Received is set.
//

// For documentation purposes here just a repetition of the defines in dcc_receiver.h
// #define MAX_DCC_SIZE  6
// typedef struct
//   {
//     unsigned char size;               // 3 .. 6, including XOR
//     unsigned char dcc[MAX_DCC_SIZE];  // the dcc content
//   } t_message;

t_message incoming;

volatile t_message local;


struct
    {
        unsigned char state;                    // current state
        unsigned char bitcount;                 // current bit
        unsigned char bytecount;                // pointer to current byte
        unsigned char accubyte;                 // location for bit stuffing
        signed char dcc_time;                   // integration time for dcc (only sampling code)
                                                // we start with -7 -> all values >= indicate a zero
        unsigned char filter_data;              // bitfield for low pass data
    } dccrec;

// some states:
#define RECSTAT_WF_PREAMBLE  0
#define RECSTAT_WF_LEAD0     1
#define RECSTAT_WF_BYTE      2
#define RECSTAT_WF_TRAILER   3
#define RECSTAT_WF_SECOND_H  4                  // nur bei der neuen!!!


#define RECSTAT_DCC          7   



// ISR(INT0) loads only a register and stores this register to IO.
// this influences no status flags in SREG.
// therefore we define a naked version of the ISR with
// no compiler overhead.

ISR(DCC_Interrupt_Vector) 
{


#if defined ENHANCED_PROCESSOR
    TC0_Control_Register_B |= (T0_PRESCALER_BITS);  // Start Timer 0
#else 
  TC0_Control_Register_A  = (0 << FOC0)             // force output: 0=not
                          | (0 << WGM00)            // wgm = 00: normal mode, top=0xff
                          | (0 << COM01)            // com = 00: normal mode, pin operates as usual
                          | (0 << COM00) 
                          | (0 << WGM01)            // 
                          | (T0_PRESCALER_BITS);    //   = run 
#endif  
}


const unsigned char copy[] PROGMEM = {"OpenDecoder2.2"};

#if (TARGET_HARDWARE == OPENDECODER22GBM)
volatile unsigned char new_adc_requested;    // Flag to signal new ADC conversion should start 
#endif



ISR(TIMER0_OVF_vect)
  {
    #define mydcc (Recstate & (1<<RECSTAT_DCC))

    // read asap to keep timing!
    if (DCCIN_STATE) Recstate &= ~(1<<RECSTAT_DCC);  // if high -> mydcc=0
    else             Recstate |= 1<<RECSTAT_DCC;    

    // Stop the timer
    TC0_Control_Register_B = (0 << CS02)		// cs02.01.00 : 0  0  0 = Timer0: stopped
                           | (0 << CS01)		//            : 0  0  1 = run 1:1
                           | (0 << CS00);		//            : 0  1  0 = run with prescaler 8


    // Interrupt occurs at MAX+1 (=256)
    // set Timer Value to 256 - (3/4 of period of a one) -> this is a time window of 116*0,75=87us
    // minus 10 us for safety
    
    TCNT0 = 256L - T77US;  

    // Next lines added by AP for GBM
    // Start new ADC in case the ADC read process (defined in occupancy.c) is ready 
    // We start new AD conversions in case mydcc is set
    // In that case the J signal is high compared to K (the ground)
    // But, since the opto-coupler inverses the signal, the DCC INT1 signal is zero
#if (TARGET_HARDWARE == OPENDECODER22GBM)
    if (new_adc_requested) 
    {
      if (mydcc) 
      {
        ADCSRA |= (1 << ADSC);         // Start the new ADC measurements
        new_adc_requested = 0;
      }
    }
#endif
    
    
    dccrec.bitcount++;

    if (Recstate & (1<<RECSTAT_WF_PREAMBLE))            // wait for preamble
      {                                       
        if (mydcc)                                      // thus DCC signal is 0 
          {
            if (dccrec.bitcount >= 10)                  // more than 10 ones => preamle
              {
                Recstate = 1<<RECSTAT_WF_LEAD0;            
              }
          }
        else
          {
            dccrec.bitcount=0;
          }
      }
    else if (Recstate & (1<<RECSTAT_WF_LEAD0))          // wait for leading 0
      {
        if (mydcc)
          {                                             // still 1, wait again
          }
        else
          {
            dccrec.bytecount=0;
            Recstate = 1<<RECSTAT_WF_BYTE;
            dccrec.bitcount=0;
            dccrec.accubyte=0;
          }
      }
    else if (Recstate & (1<<RECSTAT_WF_BYTE))           // wait for byte
      {
        unsigned char my_accubyte;
        my_accubyte = dccrec.accubyte << 1;
        if (mydcc)
          {
            my_accubyte |= 1;
          }
        dccrec.accubyte = my_accubyte;
        /* dccrec.accubyte = dccrec.accubyte << 1;
        if (mydcc)
          {
            dccrec.accubyte |= 1;
          }
         */

        if (dccrec.bitcount==8)
          {
            if (dccrec.bytecount == MAX_DCC_SIZE)       // too many bytes
              {                                         // ignore message
                Recstate = 1<<RECSTAT_WF_PREAMBLE;   
              }
            else
              {
                local.dcc[dccrec.bytecount++] = dccrec.accubyte;
                Recstate = 1<<RECSTAT_WF_TRAILER; 
              }
          }
      }
    else if (Recstate & (1<<RECSTAT_WF_TRAILER))        // wait for 0 (next byte) 
      {                                                 // or 1 (eof message)
        if (mydcc)
          {  // trailing "1" received
            Recstate = 1<<RECSTAT_WF_PREAMBLE;
            dccrec.bitcount=1;

            if (semaphor_query(C_Received))
              {
                // panic - nobody is reading the messages :-((
              }
            else
              {                                         // copy from local to global
                unsigned char i;
                for (i=0; i<MAX_DCC_SIZE; i++)
                  {
                     incoming.dcc[i] = local.dcc[i];
                  }
                incoming.size = dccrec.bytecount;
                semaphor_set(C_Received);                   // ---> tell the main prog!
              }
            
          }
        else
          {
            Recstate = 1<<RECSTAT_WF_BYTE;
            dccrec.bitcount=0;
            dccrec.accubyte=0;
          }
      }
    else
      {
        Recstate = 1<<RECSTAT_WF_PREAMBLE;
      }
  }


