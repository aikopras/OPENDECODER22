//------------------------------------------------------------------------
//
// file:      rs_bus_basic.c
//
// purpose:   Physical layer routines for the RS feedback bus
//
// This source file is subject of the GNU general public license 2,
// that is available at the world-wide-web at
// http://www.gnu.org/licenses/gpl.txt
//
// history:   2010-11-10 V0.1 Initial version
//            2011-02-06 V0.2 First complete production version
//
//------------------------------------------------------------------------

#include <stdlib.h>
#include <stdbool.h>
#include <inttypes.h>
#include <avr/pgmspace.h>        // put var to program memory
#include <avr/io.h>              // needed for UART
#include <avr/eeprom.h>
#include <avr/interrupt.h>
#include <string.h>
#include <util/parity.h>

#include "global.h"              // global variables
#include "config.h"              // general definitions the decoder, cv's
#include "hardware.h"            // port definitions for target

#include "main.h"
#include "rs_bus_hardware.h"

//--------------------------------------------------------------------------------------
//
// This code can be used to send feedback information from decoder to master station via
// the RS-bus. This code implements the physical layer routines (electrical characteristics).
// For details on the operation of the RS-bus, see: 
// http://www.der-moba.de/index.php/RS-R%C3%BCckmeldebus
//
// The basic operation is as follows.
// An interrupt (INT0) is raised at every transition of the RS-bus. If the master is polling, such
// interrupt occurs every 200 micro-seconds. One complete polling cyclus consists of 130
// interrupts. At each interrupt, the "RS_address_polled" variable gets incremented by the INT0-ISR.
// Once all feedback modules are polled, the master is idle for 7 ms. Such waiting period 
// allows all feedback modules to synchronize (which means, in our case, the RS_address_polled
// variable is reset to zero). To detect this idle timer, a timer of roughly 1 ms is running. 
// The ISR of this timer maintains a counter (T_RS_Idle), which is reset (by the INT0-ISR)  
// everytime a transition is detected at the RS-bus. If the counter exceeds 4, we know the  
// master station has been idle for more than 4 ms.
// The timer ISR sets, next to resetting the RS_address_polled variable, also maintains the
// "RS_Layer_1_active" variable, to indicate a valid (or invalid) RS-bus signal is received.
//
// Feedback modules may send information back to the master once their address is called. 
// The length of such information is 9 bits, and takes around 1,875 ms (4800 baud).
// During that period no RS-bus transistions will occur, which means that the T_RS_Idle counter 
// will not be resetted. Therefore the length of the timing interval to detect if the master  
// is idle, should be between 1,875 ms and 7 ms. A value of 4 ms therefore seems save.
//
// To send information back to the master, the proces that uses these basic RS-bus routines sets  
// the RS-bus address (RS_Addr2Use), assembles the information byte (RS_data2send) and, 
// once ready, sets the "RS_data2send_flag". 
// (Using a flag which the AVR can set within a single instruction, has as advantage that either
// all or none of the data will be send; it is not possible that the hardware starts sending data
// while the information byte is still being modified.) 
// The flag is tested, and the actual data transfer is performed, by the RS-bus ISR (INT0-ISR). 
// By initiating the actual data transfer from within this ISR, we ensure that:
// 1) data is send immediately after the feedback module gets its turn, and 
// 2) it is not possible that more than one byte is send per cycle.
//
// Note that the information byte (RS_data2send) should be filled by the process that calls
// these basic RS-bus routines in a way that conforms to the RS-bus specification. Thus the
// parity, TT-bits, nibble and four data bits should be filled by the calling process; the  
// basic routines within this file take only care of the "physical layer" of the RS-bus.
//
// After start-up, or after the master station resets, the process in rs_bus_messages.c that calls our 
// basic RS-bus routines should also connect to the master. To signal that such connect is needed,
// the RS_Layer_2_connected flag is used. Upon start-up, this flag is set to zero (not connected).
// This flag is also set to zero if the RS-bus master resets (which the master indicates
// by sending a puls of 88 ms, followed by silence period of roughly 562 ms.
//
// FYI: if no feedback module sends information, one complete polling cyclus takes 33,1 ms. 
// On the other hand, if all feedback modules send information, one polling cyclus takes
// 33,1 + 128 * 1,875 = 273,1 ms. Since feedback modules can only send half of their data 
// (one nibble) during one polling cyclus, roughly 550 ms are needed to send all changes.
// 
// Used hardware (8535, 16A, 32A, 164A, 324A, 644A and pin compatible AVRs):
// - PD2=INT0: used to receive RS-bus information from the command station
// - PD1=TXD/TXD0: used to send RS-bus information to the command station (UART / UART2)
// - 8-bit Timer/Counter2
//
//--------------------------------------------------------------------------------------

// This software should run on the older AVRs (8535, 16 & 32), as well as their enhanced versions
// (164A, 324A and 644). For more info, see also Atmel's application note AVR505 (Table 7.1).
// To allow the same software to run on multiple AVRs, we define a number of processor specific
// aliases below. In the remainder of the software we refer to these aliases.
// Note: ENHANCED_PROCESSOR is defined in hardware.h 

// Interrupt specific settings
#if defined ENHANCED_PROCESSOR
  #define Interrupt_Select_Register			EIMSK    		// External Interrupt Mask Register
  #define Interrupt_Control_Register			EICRA   		// External Interrupt Control Register
#else 
  #define Interrupt_Select_Register			GICR    		// General Interrupt Control Register
  #define Interrupt_Control_Register			MCUCR   		// MCU Control Register
#endif

// Timer 2 specific settings
#if defined ENHANCED_PROCESSOR
  #define TC2_Compare_Match_Vect			TIMER2_COMPA_vect	// Interrupt vector
  #define TC2_Output_Compare_Register			OCR2A			// Register
  #define TC2_Interrupt_Mask_Register			TIMSK2			// Register
  #define TC2_Control_Register_A			TCCR2A			// Register
  #define TC2_Control_Register_B			TCCR2B			// Register
  #define TC2_Output_Compare_Match_Interrupt_Enable	OCIE2A 			// Bit definition
#else 
  #define TC2_Compare_Match_Vect			TIMER2_COMP_vect
  #define TC2_Output_Compare_Register			OCR2
  #define TC2_Interrupt_Mask_Register			TIMSK
  #define TC2_Control_Register_A			TCCR2			// Note: A and B are
  #define TC2_Control_Register_B			TCCR2			// here the same register
  #define TC2_Output_Compare_Match_Interrupt_Enable	OCIE2 
#endif

// UART specific settings
#if defined ENHANCED_PROCESSOR
  #define USART_Data_Register				UDR0			// Register
  #define USART_Control_and_Status_Register_A		UCSR0A			// Register
  #define USART_Control_and_Status_Register_B		UCSR0B 			// Register
  #define USART_Control_and_Status_Register_C		UCSR0C 			// Register
  #define USART_Transmit_Enable_Bit			TXEN0			// Bit definition 
  #define USART_Data_Register_Empty			UDRE0			// Bit definition
  #define USART_Character_Size_0			UCSZ00
  #define USART_Character_Size_1			UCSZ01
  #define USART_Baud_Rate_Registers_Low			UBRR0L
  #define USART_Baud_Rate_Registers_High		UBRR0H
#else 
  #define USART_Data_Register				UDR     // send data to this register!
  #define USART_Control_and_Status_Register_A		UCSRA	// for flow control purposes
  #define USART_Control_and_Status_Register_B		UCSRB   // Controls transmission circuitry 
  #define USART_Control_and_Status_Register_C		UCSRC   // Controls transmission circuitry 
  #define USART_Transmit_Enable_Bit			TXEN    // Turns on transmission circuitry 
  #define USART_Register_Select				URSEL   // Selects between UBRRH / UCSRC
  #define USART_Data_Register_Empty			UDRE	// for flow control purposes
  #define USART_Character_Size_0			UCSZ0   // Character size bit 0
  #define USART_Character_Size_1			UCSZ1   // Character size bit 1
  #define USART_Baud_Rate_Registers_Low			UBRRL   // lower 8-bits of the baud rate value
  #define USART_Baud_Rate_Registers_High		UBRRH   // Higher 8-bits of the baud rate value
#endif    

//--------------------------------------------------------------------------------------
//
// Define global variables that provide the interface between rs_bus_hardware and rs_bus
//
//--------------------------------------------------------------------------------------
volatile unsigned char RS_Layer_1_active;    // Flag to signal valid RS-bus signal 
volatile unsigned char RS_Layer_2_connected; // Flag to signal slave must connect to the master
volatile unsigned char RS_data2send_flag;    // Flag that this feedback module wants to send data
volatile unsigned char RS_data2send;         // Actual data byte that will be send over the RS-bus

// local variables
volatile unsigned char RS_address_polled;    // Address of RS-bus slave that is polled now 
volatile unsigned char T_RS_Idle;            // To detect if command station is idle (> 4 ms)
volatile unsigned char T_RS_Inactive;        // To detect if command station is inactive (> 200 ms)

//--------------------------------------------------------------------------------------
//
// Define Interrupt Service routines (ISR) for INT0 and for Timer2
//
//--------------------------------------------------------------------------------------
ISR(INT0_vect)
{
  // This ISR is called whenever a transistion is detected on the RS-bus (INT0).
  // Such transistion indicates that the next feedback decoder is allowed to send information.
  // This ISR therefore increments the "RS_address_polled" variable, which corresponds to the
  // address of the feedback decoder (with offset 1) that is allowed to send next.
  // This ISR also resets T_RS_Idle, indicating that the command station is not idle.
  if (RS_data2send_flag)
  {
    if ((RS_Addr2Use == RS_address_polled) & (RS_Layer_1_active))
     { 
       // We have data to send, it is our turn and the RS-bus is operating
       // Note: we must test RS_Layer_1_active, to ensure we skip the first initialisation cycle
       if (RS_Addr2Use > 0) USART_Data_Register = RS_data2send; 
       // Note: we could have exercised flow control over the output port by including:
       // while ((USART_Control_and_Status_Register_A & (1 << USART_Data_Register_Empty)) == 0) {};
       // In case of the RS-bus, such check is not needed, however. 
       RS_data2send_flag = 0;
     }
     else if (RS_Addr2Use > 128) {RS_data2send_flag = 0;} // drop data for impossible addresses
  }
  RS_address_polled ++;		// Address of slave that gets his turn next 
  T_RS_Idle = 0;		// Reset the counter since the command station is not idle now  
} 

ISR(TC2_Compare_Match_Vect)
{
  // This ISR is called whenever Timer2, which is set to roughly 1 ms, fires
  // This ISR increases the counter T_RS_Idle, which is reset to zero, however, each time the 
  // INT0-ISR detects a transition on the RS-bus. If there are no problems, T_RS_Idle will only 
  // increase beyond 4 (ms) if the command station is idle. To check if the signal from the master
  // is 100%, we'll check the RS_address_polled variable, which is incremented each time 
  // the INT0-ISR is called. If 130 INT0 interrupts have occured, the signal is indeed 100%.
  // Note that the main purpose of the command station being idle, is to allow feedback decoders
  // to synchronize their variables.
  TCNT2 = 0;				// Reset counter 2 (this counter)
  T_Sample ++;				// Interval (in ms) between successive AD conversions (used in adc_hardware.c)
  T_DelayOff ++;			// Interval used for delaying delay_off messages (which goes in steps of 10ms)
  T_RS_Inactive ++;			// Counter to determine if the RS-bus master is inactive / resets
  T_RS_Idle ++;				// Time since last RS-bus transition  
  if (T_RS_Idle > 4) {			// The command station is idle
    T_RS_Idle = 0;
    if (RS_address_polled == 130) {
      RS_Layer_1_active = 1;		// One complete RS-bus polling cycle performed. Good!
      T_RS_Inactive = 0; 		// Since RS-master is functioning, reset counter
    }  
    else {RS_Layer_1_active = 0;}
    RS_address_polled = 0;
  }  
  if (T_RS_Inactive >= 200) {		// if 200 ms passed, the master is inactive or resets
    RS_Layer_1_active = 0;
    RS_Layer_2_connected = 0; 
    RS_data2send_flag = 0;		// flag must be cleared, to ensure calling process will not
    T_RS_Inactive = 0; 		   	// wait forever (deadlock). Note: data may get lost!
  }
} 


//--------------------------------------------------------------------------------------
//
// Define initialisation routines
//
//--------------------------------------------------------------------------------------

void init_timer2(void)
{
  // See for example Figure 17-2 in ATMega 16A manual
  // The following registers must be initialized:
  // - Timer/Counter Control Register (TCCR2 / TCCR2A & TCCR2B)
  // - Timer/Counter (TCNT2)
  // - Output Compare Register (OCR2 / OCR2A)
  // TCNT2 increases, till it reaches OCR2(A)
  // IF (TCNT2 == OCR2(A)) => interrupt
  //
  // The following settings are needed
  // 1) calculate how far Timer2 should count before an interrupt is raised
  // 2) determine a prescaler value (to be stored later in TCCR2 / TCCR2B)
  // 3) check if this value is between 32 and 254 (note: we have an 8 bit timer!)
  // 4) set this value in the Output Compare Register
  // 5) enable interrupts for Timer/Counter2 Compare Matches (OCIE2 / OCIE2A)
  // 6) initialise the Timer/Counter Control Register
  // 7) Initialise a counter to determine if the master is inactive / resets
  // 8) initialise this Timer/Counter
  //
  // Step 1: Define timer period
  #define time_microsonds 1000L		// timer fires after 1 ms
    // Step 2: Determine prescaler
  #define T2_PRESCALER   256    // may be 1, 8, 32, 64, 128, 256, 1024
  #if   (T2_PRESCALER==1)
        #define T2_PRESCALER_BITS   ((0<<CS22)|(0<<CS21)|(1<<CS20))
  #elif (T2_PRESCALER==8)
        #define T2_PRESCALER_BITS   ((0<<CS22)|(1<<CS21)|(0<<CS20))
  #elif (T2_PRESCALER==32)
        #define T2_PRESCALER_BITS   ((0<<CS22)|(1<<CS21)|(1<<CS20))
  #elif (T2_PRESCALER==64)
        #define T2_PRESCALER_BITS   ((1<<CS22)|(0<<CS21)|(0<<CS20))
  #elif (T2_PRESCALER==128)
        #define T2_PRESCALER_BITS   ((1<<CS22)|(0<<CS21)|(1<<CS20))
  #elif (T2_PRESCALER==256)
        #define T2_PRESCALER_BITS   ((1<<CS22)|(1<<CS21)|(0<<CS20))
  #elif (T2_PRESCALER==1024)
        #define T2_PRESCALER_BITS   ((1<<CS22)|(1<<CS21)|(1<<CS20))
  #endif
  // Step 3: Check prescaler
  // Pre-processor check whether timer values are OK for 8 bit
  // Target Timer Count = (Input Frequency * Target Time / Prescale) - 1 
  #define T2_target_count (F_CPU / T2_PRESCALER * time_microsonds / 1000000L)
  #if (T2_target_count > 254)
    #warning T2_target_count too big, use either larger prescaler or slower processor
  #endif
  #if (T2_target_count < 32)
    #warning T2_target_count too small, use either smaller prescaler or faster processor
  #endif
  // Step 4: Initialize the Output Compare Register
  TC2_Output_Compare_Register = T2_target_count; 
  // Step 5: Enable the Timer/Counter2 Compare Match interrupt
  TC2_Interrupt_Mask_Register |= (1 << TC2_Output_Compare_Match_Interrupt_Enable);
  // Step 6: Initialize the Timer/Counter Control Register
  TC2_Control_Register_A |= (1 << WGM21);          // Configure Timer2 for CTC mode 
  TC2_Control_Register_B |= (T2_PRESCALER_BITS);   // Start Timer2
  // Step 7: Initialise a counter to determine if the master resets / is no longer active.
  // If this counter passes 200 (ms), we may conclude the master is no longer active on the RS-bus
  T_RS_Inactive = 0;
  // Step 8: Initialise the Timer/Counter
  TCNT2 = 0;  
}


void init_rs_input_interrupt(void)
{
  // Step 1: set the Global Interrupt Enable bit I, in the AVR Status Register SREG. 
  // The I- bit is set (and cleared) by the application with the SEI (and CLI) instruction.
  // In OpenDCC, calling sei() is already performed in main.c (at start) and port_engine.c
  // We can therefore skip it here.
  // sei();
  //
  // Step 2: the individual interrupt source's enable bit must be set.
  // - Set INT0 in the General Interrupt Control Register (GICR)
  // - Determine when to trigger: falling or rising edge
  //   For INT0 and INT1 this should be set in the normal MCU Control Register (MCUCR)
  Interrupt_Select_Register |= (1<<INT0);     			// Enable INT0
  Interrupt_Control_Register |= (0<<ISC00) | (1<<ISC01) ;     //  A falling edge on INT0 activates the interrupt
  // Interrupt_Control_Register |= (1<<ISC00) | (1<<ISC01) ;  //  A rising edge on INT0 activates the interrupt.
  // Triggering on the falling edge allows immediate sending after the right number of pulses
  // Step 3: initialise the variable that keeps track which address is currently being polled
  RS_address_polled = 0;    // Start with any address; the first polling cyclus will not be used 
}


void init_rs_usart(void)
{
  // Note: on AVRs with multiple UARTS, we use USART 0
  // See also UART tutorial on AVR freaks
  // Step 1: Initialise USART Control and Status Register B
  // This register is responsible for (amongst others) activating (receive and) transmission circuitry
  USART_Control_and_Status_Register_B |= (1 << USART_Transmit_Enable_Bit);
  // Step 2: Initialise USART Control and Status Register C
  // This register determines (amongst others) what type of serial format is being used
  // We use 8 bit (no parity, 1 stop bit, asynchronous mode)
  #if defined ENHANCED_PROCESSOR
    USART_Control_and_Status_Register_C |= (1 << USART_Character_Size_0)
                                        |  (1 << USART_Character_Size_1);    
  #else 
    USART_Control_and_Status_Register_C |= (1 << USART_Register_Select)		// Use URSEL bit! 
                                        |  (1 << USART_Character_Size_0)
                                        |  (1 << USART_Character_Size_1);
  #endif
  // Step 3: Set baudrate to 4800 baud
  #define USART_BAUDRATE 4800
  #define BAUD_PRESCALE (((F_CPU / (USART_BAUDRATE * 16UL))) - 1) 
  USART_Baud_Rate_Registers_Low  = BAUD_PRESCALE; // Load lower 8-bits of the baud rate value 
  USART_Baud_Rate_Registers_High = (BAUD_PRESCALE >> 8); // Load upper 8-bits of the baud rate 
} 

//************************************************************************************************
// init_RS_hardware will be directly called from main externally
//************************************************************************************************
void init_RS_hardware(void) {
  // STEP 1: init the interface variables that are used with "rs_bus_message.c" and "occupancy.c"
  RS_Layer_1_active = 0;       	// No valid RS-bus signal detected yet
  RS_Layer_2_connected = 0;	// This RS-bus slave should try to connect to the RS-bus master  
  RS_data2send_flag = 0;    	// No, we don't have anything to send yet
  RS_data2send = 0;         	// Empty our send data byte
  // STEP 2: initialise the RS bus hardware
  init_rs_usart();  
  init_rs_input_interrupt();
  init_timer2();
}



