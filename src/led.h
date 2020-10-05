//------------------------------------------------------------------------
//
// OpenDCC - OpenDecoder
//
// Copyright (c) 2007 Kufer
//
// This source file is subject of the GNU general public license 2,
// that is available at the world-wide-web at
// http://www.gnu.org/licenses/gpl.txt
// 
//------------------------------------------------------------------------
//
// file:      port_engine.h
// author:    Wolfgang Kufer
// contact:   kufer@gmx.de
// webpage:   http://www.opendcc.de
// history:   2007-02-14 V0.1 kw copied from opendecoder.c
//
//------------------------------------------------------------------------
//
// purpose:   flexible general purpose decoder for dcc
//            here: hardware defintions
//
//
//------------------------------------------------------------------------
#pragma once

void turn_led_on(void);
void turn_led_off(void);

void feedback_led(void);
void activity_led(void);
  
void flash_led_fast(unsigned char count);

void check_led_time_out(void);


