//------------------------------------------------------------------------
//
// OpenDCC - OpenDecoder2.2
//
// Copyright (c) 2007 Kufer
//
// This source file is subject of the GNU general public license 2,
// that is available at the world-wide-web at
// http://www.gnu.org/licenses/gpl.txt
// 
//------------------------------------------------------------------------
//
// file:      cv_data_switch.h   -> Note that this is an include file, and not a normal header file
// author:    Wolfgang Kufer / Aiko Pras
// history:   2007-08-03 V0.1 kw start
//            2007-08-18 V0.2 kw default for myADDR high changed to 0x80
//                               -> this means: unprogrammed    
//            2011-01-15 V0.3 ap initial values have been included for the new CVs: System and
//                               SkipEven.
//            2012-01-03 v0.4 ap New Cvs for GBM added.
//            2013-03-12 v0.5 ap The ability is added to program the CVs on the main (PoM).
//            2013-12-30 v0.6 ap File customized for switches.
//            2014-12-24 V0.7 ap SkipEven is changed into SkipUnEven
//            2020-10-05 v0.8 ap Several changes such that software can now also be programmed
//                               via the Arduino IDE. Software version updated to 0x10      
//                               Note that the clause #ifndef _CV_DATA_... / #pragma should NOT be added,
//                               since this is not a normal header (.h) file, but a piece of code that 
//                               needs to be included multiple times!
//                               It would be more logical to change the file extension to .ini, but
//                               the Arduino IDE can only deal with .c, .cpp and .h files.
//
//-----------------------------------------------------------------------------
// NOTE: Don't include an #pragma once clause!!
//
// data in EEPROM:
// Note: the order of these data corresponds to physical CV-Address
//       CV1 is coded at #00
//       see RP 9.2.2 for more information

// Content         Name         CV  Access Comment
   0x01,        // myAddrL       1  R/W    Accessory Address low (6 bits).
   0,           // cv2           2  R      not used
   15,          // T_on_F1       3  R      Hold time for relays 1 (in 20ms steps)
   15,          // T_on_F2       4  R      Same dor relays 2
   15,          // T_on_F3       5  R      Same dor relays 3
   15,          // T_on_F4       6  R      Same dor relays 4
   0x10,        // version       7  R      Software version. Should be > 7
   0x0D,        // VID           8  R/W    Vendor ID (0x0D = DIY Decoder
                                           // write value 0x0D = 13 to reset CVs to default values
   0x80,        // myAddrH       9  R/W    Accessory Address high (3 bits)
   0,           // MyRsAddress  10  R/W    RS-bus address (=Main) for this decoder (1..128 / not set: 0)
   0,           // cv11         11  R      not used
   0,           // cv12         12  R      not used
   0,           // cv13         13  R      not used
   0,           // cv14         14  R      not used
   0,           // cv15         15  R      not used
   0,           // cv16         16  R      not used
   0,           // cv17         17  R      not used
   0,           // cv18         18  R      not used
   1,           // CmdStation   19  R/W    To handle manufacturer specific address coding
						// 0 - Standard (such as Roco 10764)
						// 1 - Lenz
   0,           // RSRetry      20  R/W    Number of RS-Bus retransmissions
   1,           // SkipUnEven   21  R/W    Only Decoder Addresses 2, 4, 6 .... 1024 will be used
   0,           // cv534        22  R      not used
   0,           // Search       23  R/W    If 1: decoder LED blinks
   0,           // cv536        24  R      not used
   0,           // Restart      25  R/W    To restart (as opposed to reset) the decoder: use after PoM write
   0,           // DccQuality   26  R/W    DCC Signal Quality
   0b00010000,  // DecType      27  R/W    Decoder Type
						// 0x00010000 - Switch decoder
						// 0x00010001 - Switch decoder with Emergency board
						// 0x00010100 - Servo decoder
						// 0x00100000 - Relais decoder for 4 relais
						// 0x00100001 - Relais decoder for 16 relais
						// 0x10000000 - Watchdog and safety decoder
   0,           // BiDi         28  R      Bi-Directional Communication Config. Keep at 0.
                // Config       29  R      similar to CV#29; for acc. decoders
      (1<<7)                                    // 1 = we are accessory
    | (0<<6)                                    // 0 = we do 9 bit decoder adressing
    | (0<<5)                                    // 0 = we are basic accessory decoder
    | 0,                                        // 4..0: all reserved
   0x0D,        // VID_2        30  R      Second Vendor ID, to detect AP decoders
   0,           // cv31         31  R      not used
   0,           // cv32         32  R      not used

// CVs specific for the SWITCH Decoder
   1,           // SendFB	33  R/W    Decoder will send switch feedback messages via RS-Bus 
                                           // Should be 0 if decoder sends ONLY PoM feedback (Add. 128)
   1,           // AlwaysAct	34  R/W    If set, decoder will activate coil / relays for each DCC command
                                           // received (since this generates feedback, is needed by Railware)
                                           // If zero, will not activate coil / relays if it is already 
                                           // in the requested position
