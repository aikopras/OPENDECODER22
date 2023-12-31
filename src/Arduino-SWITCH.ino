/* Arduino-SWITCH.ino
 * 2020/10/05 AP 
 * Software version (CV7) = 0x10 (set in cv_data_switch.h / cv_data_relays4.h)
 *  
 * The code has been tested on the following boards: 
 * - https://easyeda.com/aikopras/switch-decoder
 * - https://easyeda.com/aikopras/relays-4-decoder
 * 
 * 
 * BUID AND UPLOAD 
 * ===============
 * The code for the decoder was written in "pre-Arduino" times, and can be compiled using "make".
 * 
 * However, the code can also be compiled, linked and uploaded using the Arduino IDE. 
 * To use the Arduino IDE, it is necessary to install the Mightycore boards for ATMEGA 16 etc: 
 * See: https://github.com/MCUdude/MightyCore for installation and usage instructions.
 * 
 * The following IDE "Tools" settings are needed to upload the code to these boards:
 * - Mightycore => ATmega16
 * - Clock: External 11.0592 MhZ
 * - BOD: 4,0V
 * - Compiler LTO: "LTO diabled" or "LTO enabled"
 * - Pinout: Standard pinout (other pinnouts will work as well, however)
 * - Bootloader: no bootloader
 * Since the boards have no USB ports, uploading has to done using an external programmer 
 * (like USBASP), connected to the (16 pin) ISP connector on the decoder board.

 * To initialise the fuse bits, select on the Arduino IDE "Tools => Burn Bootloader".
 * Although no bootloader was selected, this command will set the fuse bits.
 * To flash the program, select on the Arduino IDE "Sketch => Upload"
 * 
 * 
 * FIRST USE
 * =========
 * After the fuse bits are set and the program is flashed, the decoder is ready to use.
 * Note that the Arduino IDE is unable to flash the decoder's configuration variables to EEPROM.
 * Thefore the main program performs a check after startup and, if necessary, initialises the EEPROM.
 * For this check the value of two CVs is being tested: VID and VID_2 (so don't change these CVs).
 * After the EEPROM has been initialised, the LED is blinking to indicate that it expects an
 * accessory (=switch) command to set the RS-Bus address. Push the PROGRAM button, and 
 * the next accessory (=switch) address received will be used as address.
 *
 *
 * CONFIGURATION VARIABLES
 * =======================
 * The default values of the Configuration Variables (CVs) can be modified in the file cv_data_switch.h or cv_data_relays4.h
 * In particular the decoder type (CV27: DecType) can be changed, for example to select either a switch or a relays decoder.
 * 
 * Configuration Variables can also be changed using Programming on the Main (PoM). 
 * Unfortunately many Command Stations, including the LENZ LZV100, do not support PoM for accessory 
 * decoders. Therefore this software implemenets PoM for loco decoders.
 * The feedback decoder therefore listens to a LOCO  address that is equal to the Decoder address + 6999. 
 * Transmission of PoM SET commands conforms to the NMRA standards.
 * PoM VERIFY commands do NOT use railcom feedback messages and therefore do NOT conform to the NMRA 
 * standards. Instead, the CV Value is send back via the RS-Bus (a proprietary solution).
 * 
 * For MAC users an OSX program to read and modify CVs can be downloaded from:
 * https://github.com/aikopras/Programmer-Decoder-POM
 *
 *
 * NOTES
 * =====
 * The default CV values can be resored by pushing the PROGRAM button for more tham 5 seconds.
 * 
 * ALthough you can modify this code, it is recommended to NOT include and use Arduino libraries.
 * Resources (such as Timer 0), which are needed by the Arduino software, are already used by this software. 
 * ALthough changes can be made, porting this sofware to C++ is not trivial. 
 * 
 * Note also that this .ino file should NOT include a call to setup() and loop(),
 * since the main loop is already called from main() (in main.c)
 * 
 */
