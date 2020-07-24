//*****************************************************************************************************
//
// file:      dcc_decode.h
// purpose:   Definition of dcc_decode routines
//
// This source file is subject of the GNU general public license 2, that is available at:
// http://www.gnu.org/licenses/gpl.txt
//
// history:   2006-02-14 V0.1 wk: start
//            2007-04-27 V0.4 wk: changed return codes
// 	      2013-03-25 V0.5 ap: comletely modified structure
//
//*****************************************************************************************************

void init_dcc_decode(void);
void analyze_message(t_message *new);       // Sets the global CmdType variable plus possible others 



