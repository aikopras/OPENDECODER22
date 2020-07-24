//*****************************************************************************************************
//
// file:      global.c
// purpose:   Definition of global variables
//
// This source file is subject of the GNU general public license 2, that is available at:
// http://www.gnu.org/licenses/gpl.txt
//
// history:   2013-03-25 V0.1 Initial version
//
//
// Basic decoder structure:
// 1) Each decoder has a decoder address
// 2) A decoder has generally 4 devices (=switches, relays). More are possible, however
// 3) Each device has 2 gates (=coils)
// 4) Each gate can be active or inactive
// 
// 
// RecDecAddr (Received Decoder Address)
// - 0..255  if XpressNet is being used
// - 0..511  theoretical maximum value for BASIC accessory decoders
// - 0..2047 theoretical maximum value for EXTENDED accessory decoders
// - INVALID_DEC_ADR is decoder has not been initialised, or has invalid value
// 
// RecDecPort (Received Decoder Port / initialised and used in dcc_decode.c)
// - Range = 0 .. 3
// 
// TargetDevice (Number of the switch / relay this DCC command is targetted at)
// - Range = 0 .. (NUMBER_OF_DEVICES - 1)
// - For normal switch / relays4 decoders, the range will be 0..3
// - In case of relays16 decoders, the range is 0..7
// 
//// TargetGate (Received Decoder Gate)
// - Range = 0..1 
//
// RecLocoAddr
// - Range: 0..10238 (in theory)
// - Acceptable values: LOCO_OFFSET .. LOCO_OFFSET + My_RS_Addr (or My_Dec_Addr)
//  
//  
// Example of an accesory decoder message: 
// LH100 => RecDecAddr RecDecPort TargetGate:
//    1- =>     0          0          0
//    1+ =>     0          0          1
//    2- =>     0          1          0
//    2+ =>     0          1          1
//    3- =>     0          2          0
//    3+ =>     0          2          1
//    4- =>     0          3          0
//    4+ =>     0          3          1
//    5- =>     1          0          0
//    5+ =>     1          0          1
// 1024+ =>   255          3          1
// Note 1: The example assumes NUMBER_OF_DEVICES = 4
// Note 2: The example assumes that, since LZV100 generated the command, CV26 was used
//
//
//*****************************************************************************************************
//********************************************* INCLUDES **********************************************
//*****************************************************************************************************
#include <stdlib.h>
#include <stdbool.h>
#include <inttypes.h>
#include "global.h"

//*****************************************************************************************************
//*********************************** DEFINITION OF GLOBAL VARIABLES **********************************
//*****************************************************************************************************
// Decoder addresses
unsigned int  My_Dec_Addr;	 // Can be used for switches and relays (depending on hardware)
				 // Range: 0..511 (0..255 if Xpressnet is used)
				 // Derived from CV1 and CV9
unsigned char My_RS_Addr;	 // Base RS-bus address of this feedback module
				 // Derived from CV10
                                 // Range: 1..128 / 0 if not initialized
unsigned char RS_Addr2Use;	 // RS-bus address used by RS bus hardware routines / over the RS-bus
				 // This is the address seen by the command station (RS-bus receiver)
				 // Is equal to, or 1 higher (in case of SkipUnEven) than My_RS_Addr
unsigned int  My_Loco_Addr;	 // Decoder listens to loco address to facilitate PoM and F1..F4
				 // Derived from LOCO_OFFSET and My_RS_Base_Addr / My_Dec_Addr
				 // If My_Dec_Addr is invalid, My_Loco_Addr will become LOCO_OFFSET - 1

// Data extracted from received DCC packets.
unsigned char CmdType;		 // Received DCC command. For possible values see #defines above
// The next (accessory decoder) variable will be used by initialisation code
unsigned int  RecDecAddr; 	 // Decoder Address as contained in the received DCC packet.
// The next variables will be used by switch / relays specific code
unsigned int  TargetDevice;	 // Targetted Device. A Device can be a switch, relay etc.
unsigned int  TargetGate;	 // Targetted coil within that Port. Usually + or - / green or red
unsigned char TargetActivate;    // Coil activation (value = 1) or deactivation (value = 0) 
// The next variables will be used for CV programming code
unsigned int  RecLocoAddr; 	 // Received Loco Address. See below
unsigned int  RecCvNumber; 	 // Configuration Variable to change. Range [0... ]
unsigned char RecCvData;	 // CV Value te set. Range [0..255]
enum CvOpType RecCvOperation;	 // CV Operation (most common: write or verify)

// Other shared data
unsigned char DccSignalQuality;	// Counts number of checksum errors 
unsigned char MyConfig;	        // The kind of accessory decoder we are. Basic = 0 / Extended = 1
unsigned char MyType;	        // 48: normal / 49: reverser / 50: relays / 52: Speed
unsigned char Have_Feedback;	// Decoder can determine switch positions and send RS-Bus feedback messages


