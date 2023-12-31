//------------------------------------------------------------------------
//
// file:      rs_bus_messages.h
//
// purpose:   Header file for the RS messages feedback bus routines
//
// This source file is subject of the GNU general public license 2,
// that is available at the world-wide-web at
// http://www.gnu.org/licenses/gpl.txt
//
// history:   2010-11-10 V0.1 Initial version
//            2013-04-20 V0.2 Only send routines kept - derived from previolus rs_bus_port.h
//
//--------------------------------------------------------------------------------------
#pragma once

// Calling:
// - format_and_send_RS_data_nibble(value) is called from ... 
// - send_CV_value_via_RSbus (value) is called from cv_pom.c

void format_and_send_RS_data_nibble(unsigned char data_byte);
void send_CV_value_via_RSbus(unsigned char value);

