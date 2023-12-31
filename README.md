# OPENDECODER22
Switch / Relays decoder for the DCC (Digital Command Control) system with RS-Bus feedback.
Compared to other switch decoder projects, this decoder has the following unique features:
* Supports feedback of the actual switch position via the RS-Bus; manual changes of the position of a switch will thus be detected. For details on how this works, see the description on the [OpenDecoder website](https://www.opendcc.de/elektronik/opendecoder/opendecoder_sw_rm_e.html)
* Has an extensive set of configurable Configuration Variables (CVs) that can be easily modified (see below).

The decoder can operate in two possible modes:
* Decoder for 4 switches
* Decoder for 4 relays
The file [global.h](src/global.h) defines which DECODER_TYPE we have, thus how the software should behave.

The software is written in C and runs on ATMEGA16A and similar processors (32A, 164A, 324A, 644P).
It is an extension of the [Opendecoder](https://www.opendcc.de/index_e.html) project, and written in "pre-Arduino times".
It can be compiled, linked and uploaded using the [<b>Makefile</b> file](/src/Makefile) in the src directory, or via the Arduino IDE. Instructions for using the Arduino IDE can be found in the [<b>Arduino-SWITCH.ino</b> file](/src/Arduino-SWITCH.ino). Note that you have to rename the /src directory into "Arduino-SWITCH" before you open the .ino file.


## Hardware
The hardware and schematics can be downloaded from my EasyEda homepage; different PCBs are available:
* [Decoder for 4 switches](https://easyeda.com/aikopras/switch-decoder),
* [Decoder for 4 relais](https://easyeda.com/aikopras/relays-4-decoder)
* [RS-Bus piggy-back print](https://easyeda.com/aikopras/rs-bus-tht)
* [SMD version of the RS-Bus piggy-back print](https://easyeda.com/aikopras/rs-bus-smd)
A description of this decoder and related decoders can be found on [https://sites.google.com/site/dcctrains](https://sites.google.com/site/dcctrains).


## First use
After the fuse bits are set and the program is flashed, the decoder is ready to use.
Note that the Arduino IDE is unable to flash the decoder's configuration variables to EEPROM. Therefore the main program performs a check after startup and, if necessary, initialises the EEPROM. For this check the value of two CVs is being tested: VID and VID_2 (so don't change these CVs). After the EEPROM has been initialised, the LED is blinking to indicate that it expects an accessory (=switch) command to set the switch address. Push the PROGRAM button, and the next accessory (=switch) address received will be used as the switch address (1..1024)


## Configuration Variables (CVs)
The operation of the software can be controlled via Configuration Variables (CVs). The default CV values for the switch decoder can be modified in the file [cv_data_switch.h](src/cv_data_switch.h), for the relays decoder see [cv_data_relays4.h](src/cv_data_relays4.h).

Configuration Variables can be modified using a programming track.

Configuration Variables can also be modified using Programming on the Main (PoM).
Unfortunately many Command Stations, including the LENZ LZV100, do not support PoM for accessory decoders. Therefore this software implements PoM for LOCO decoders. The feedback decoder therefore listens to a LOCO  address that is equal to the RS-Bus address + 6999. Transmission of PoM SET commands conforms to the NMRA standards.
PoM VERIFY commands do use railcom feedback messages and therefore do NOT conform to the NMRA standards. Instead, the CV Value is send back via the RS-Bus using address 128 (a proprietary solution).

For MAC users an easy to use OSX program to read and modify CVs can be downloaded from: [https://github.com/aikopras/Programmer-Decoder-POM](https://github.com/aikopras/Programmer-Decoder-POM).
