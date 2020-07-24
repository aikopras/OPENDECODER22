//************************************************************************************************
//
// file:      rs_bus_hardware.h
//
// purpose:   Physical layer routines for the RS feedback bus
//
// This source file is subject of the GNU general public license 2,
// that is available at the world-wide-web at
// http://www.gnu.org/licenses/gpl.txt
//
// history:  2010-11-10 ap V0.1 Initial version
// 	     2010-11-10 ap V0.2 RS-bus defines have been moved to here (made global)
//
//************************************************************************************************
// The following #define may be changed: the kind of RS-bus module
#define RS_BUS_TYPE     0       // 0: switch decoder with feedback / 1: feedback decoder


//************************************************************************************************
// DO NOT MODIFY BELOW THIS LINE
// Define the bits of the RS-bus "packet" (1 byte)
// Note: least significant bit (LSB) first. Thus the parity bit comes first,
// immediately after the USART's start bit. Because of this (unusual) order, the USART hardware
// can not calculate the parity bit itself; such calculation must be done in software.
#define DATA_0          7       // feedback 1 or 5
#define DATA_1          6       // feedback 2 or 6
#define DATA_2          5       // feedback 3 or 7
#define DATA_3          4       // feedback 4 or 8
#define NIBBLE          3       // low or high order nibble
#define TT_BIT_0        2       // this bit must always be 0
#define TT_BIT_1        1       // this bit must always be 1
#define PARITY          0       // parity bit; will be calculated by software


//************************************************************************************************
// Global Data: 
volatile unsigned char RS_Layer_1_active;    // Flag to signal valid RS-bus signal 
volatile unsigned char RS_Layer_2_connected; // Flag to signal slave must connect to the master
volatile unsigned char RS_data2send_flag;    // Flag that this feedback module wants to send data
volatile unsigned char RS_data2send;         // Actual data byte that will be send over the RS-bus

volatile unsigned char T_Sample;             // Used by adc_hardware as interval between AD conversions
volatile unsigned char T_DelayOff;           // Used by adc_hardware as to time delay before OFF message

//************************************************************************************************
// Hardware initialisation and ISR routines
void init_RS_hardware(void);
