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
// file:      timer1.h
// author:    Wolfgang Kufer
// contact:   kufer@gmx.de
// webpage:   http://www.opendcc.de
// history:   2007-02-14 V0.1 kw copied from opendecoder.c
//
//------------------------------------------------------------------------
//
// purpose:   flexible general purpose decoder for dcc
//            here: timer1 defintions
//
//
//------------------------------------------------------------------------
#pragma once

// Called by main
void init_timer1(void);

// called by RS_Send()
unsigned char time_for_next_feedback(void);
unsigned char start_up_phase(void);
