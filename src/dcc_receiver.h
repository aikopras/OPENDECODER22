//------------------------------------------------------------------------
//
// OpenDCC - OpenDecoder
//
// Copyright (c) 2006 Kufer
//
// This source file is subject of the GNU general public license 2,
// that is available at the world-wide-web at
// http://www.gnu.org/licenses/gpl.txt
// 
//------------------------------------------------------------------------
//
// file:      dcc_receiver.h
// author:    Wolfgang Kufer
// contact:   kufer@gmx.de
// webpage:   http://www.opendcc.de
// history:   2006-02-14 V0.1 kw start
//
//------------------------------------------------------------------------
//
// purpose:   flexible general purpose decoder for dcc
//
//------------------------------------------------------------------------
//
// howto:     Step 1: call init_dcc_receiver()
//            Step 2: every time a new message is received, the
//                    semaphor_set is called with C_Received = 1
//            Step 3: The host program can now read the message incoming;
//                    The message must be read within the next
//                    2ms and must be checked by the host,
//                    dcc_receiver makes only the physical layer.
//
#pragma once


#define MAX_DCC_SIZE  6
typedef struct
  {
    unsigned char size;               // 3 .. 6, including XOR
    unsigned char dcc[MAX_DCC_SIZE];  // the dcc content
  } t_message;


extern t_message incoming;         // here we deliver the incoming message

void init_dcc_receiver(void);

void activate_ACK(unsigned char time);          // make prog or feedback ack


// Added by AP             
volatile unsigned char new_adc_requested;    // Flag to signal new ADC conversion should start 
