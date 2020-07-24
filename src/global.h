//*****************************************************************************************************
//
// file:      global.h
// purpose:   Definition of global variables
//	      For a description of the various variables, see global.c
//
// This source file is subject of the GNU general public license 2, that is available at:
// http://www.gnu.org/licenses/gpl.txt
//
// history:   2013-03-25 V0.1 Initial version
//
//
//
//*****************************************************************************************************
//******************************************** DECODER TYPE *******************************************
//*****************************************************************************************************
// Define which DECODER_TYPE we have, thus how the software should behave.
// Possible values are: TYPE_SWITCH and TYPE_RELAYS4
// Depending on the DECODER_TYPE, config.c will load different CV default values
#define DECODER_TYPE	 TYPE_SWITCH
// #define DECODER_TYPE	 TYPE_RELAYS4

//*****************************************************************************************************
//****************************************** GLOBAL CONSTANTS *****************************************
//*****************************************************************************************************
// Defines
#define LOCO_OFFSET    7000      // My_Loco_Addr = LOCO_OFFSET + My_DEC_Addr (for PoM & F1..F4)
#define NUMBER_OF_DEVICES 4      // Most decoders have 4 devices (=switches, relays) with 2 gates (=coils)   
                                 // The relays16 decoder has 8 devices. Numbering starts at 0

// Last DCC command received
#define IGNORE_CMD	  0      // Command should be ignored  
#define ANY_ACCESSORY_CMD 1      // Any accessory  
#define ACCESSORY_CMD     2      // Accesory for this decoder (a decoder may have > 8 coils)  
#define LOCO_F0F4_CMD	  3      // Locomotive for F0..F4  
#define POM_CMD      	  4      // Programming on the Main (PoM) 
#define SM_CMD 	          5      // Programming in Service Mode (SM = programming track) 

// Decoder types
#define TYPE_SWITCH	  16      // Switch decoder  
#define TYPE_SERVO	  20      // Servo decoder 
#define TYPE_RELAYS4	  32      // Relays decoder for 4 relays
#define TYPE_RELAYS16	  33      // Relays decoder for 16 relays   
#define TYPE_NORMAL	  48      // Normal feedback decoder  
#define TYPE_REVERSER	  49      // Feedback decoder with reverser
#define TYPE_RELAYS	  50      // Feedback decoder with   
#define TYPE_SPEED	  52      // Feedback decoder with speed measurements  
#define TYPE_FUNCTION	  64      // Function decoder (SMD version)  
#define TYPE_WATCHDOG	 128      // Watchdog and safety decoder

// Various constants
#define INVALID_DEC_ADR	  0xFFFF // Used if decoder has not been initialised  

//*****************************************************************************************************
//******************************************** GLOBAL TYPES *******************************************
//*****************************************************************************************************

enum CvOpType 			// CV Operation Type. Used for Service mode and PoM programming
{				// same coding as NMRA
  CV_NOP          = 0b00,	// CC=00 Reserved for future use
  CV_VERIFY       = 0b01,	// CC=01 Verify byte
  CV_WRITE        = 0b11,	// CC=11 Write byte
  CV_BITOPERATION = 0b10,	// CC=10 Bit manipulation
};

//*****************************************************************************************************
//********************************** DECLARATION OF GLOBAL VARIABLES **********************************
//*****************************************************************************************************
extern unsigned int  My_Dec_Addr;	//
extern unsigned char My_RS_Addr;	// Base value derived from CV10
extern unsigned char RS_Addr2Use;	// Actual value being used. Can be 1 higher than base address
extern unsigned int  My_Loco_Addr;	// OFFSET added to My_Dec_Addr / My_RS_Base_Addr
extern unsigned char CmdType;
extern unsigned int  RecDecAddr;
extern unsigned int  TargetDevice;
extern unsigned int  TargetGate;
extern unsigned char TargetActivate;
extern unsigned int  RecLocoAddr;
extern unsigned int  RecCvNumber;
extern unsigned char RecCvData;
extern unsigned char DccSignalQuality;
extern unsigned char MyConfig;
extern unsigned char MyType;
extern unsigned char Have_Feedback;
extern enum CvOpType RecCvOperation;

