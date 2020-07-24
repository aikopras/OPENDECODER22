#include <stdlib.h>
#include <stdbool.h>
#include <inttypes.h>
#include <avr/pgmspace.h>        // put var to program memory
#include <avr/io.h>
#include <avr/eeprom.h>

void my_eeprom_write_byte(uint8_t *__p, uint8_t __value)
  {
    eeprom_write_byte(__p, __value);
  }


uint8_t my_eeprom_read_byte(const uint8_t *__p)
  {
    return(eeprom_read_byte(__p));
  }

