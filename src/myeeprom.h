// this is only a wrapper to prevent inlining from gcc
// this reduces code size dramatically!!

uint8_t my_eeprom_read_byte(const uint8_t *__p);


void my_eeprom_write_byte(uint8_t *__p, uint8_t __value);
