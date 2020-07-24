# OPENDECODER22
Switch / Relays decoder for the DCC (Digital Command Control) system with RS-Bus feedback.
Compared to other switch decoder projects, this decoder has the following unique features:
* Supports feedback of the actual switch position via the RS-Bus; manual changes of the position of a switch will thus be detected. For details on how this works, see the description on the [OpenDecoder website](https://www.opendcc.de/elektronik/opendecoder/opendecoder_sw_rm_e.html)
* Has an extensive set of configurable Configuration Variables (CVs). These variables can be modified from a MAC computer using a dedicated program (only available for MAC), via PoM (Programming on the Main) as well as from a programming track. For details of the switch decoder see [cv_data_switch.h](src/cv_data_switch.h), for the relays decoder see [cv_data_relays4.h](src/cv_data_relays4.h).

The decoder can operate in two possible modes:
* Decoder for 4 switches
* Decoder for 4 relays
The file [global.h](src/global.h) defines which DECODER_TYPE we have, thus how the software should behave.

The software is written in C and runs on ATMEGA16A and similar processors (32A, 164A, 324A, 644P). 
It is an extension of the [Opendecoder](https://www.opendcc.de/index_e.html) project, and written in "pre-Arduino times". Therefore compilation is based on a traditional [makefile](src/Makefile), and assumes availability of gcc, [avrdude](https://www.nongnu.org/avrdude/) as well as a USBasp programmer.

A description of this decoder and related decoders can be found on [https://sites.google.com/site/dcctrains](https://sites.google.com/site/dcctrains).
The hardware and schematics can be downloaded from my EasyEda homepage; different PCBs are available:
* [Decoder for 4 switches](https://easyeda.com/aikopras/switch-decoder),
* [Decoder for 4 relais](https://easyeda.com/aikopras/relays-4-decoder)
* [RS-Bus piggy-back print](https://easyeda.com/aikopras/rs-bus-tht)
* [SMD version of the RS-Bus piggy-back print](https://easyeda.com/aikopras/rs-bus-smd)
