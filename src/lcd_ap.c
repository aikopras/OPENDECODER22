/****************************************************************************
 Title	:   LCD-AP: home-brewed LCD routines 
 Author:    Aiko Pras
       
*****************************************************************************/
#define LCD_ACTIVE 1       		// Activates the LCD routines for debugging

#include <avr/io.h>
#include <stdio.h>
#include "lcd.h"				// Peter Fleury's LCD routines
#include "lcd_ap.h"


//--------------------------------------------------------------------------------------------

char string[20];
int test;

void init_lcd(void)
{
#if (LCD_ACTIVE)
  lcd_init(LCD_DISP_ON);				/* initialize display, cursor off */
  lcd_clrscr();							/* clear the screen*/
  lcd_puts("Aiko Pras");				/* displays the string on lcd*/
  lcd_gotoxy(0,1);
  lcd_puts("GBM decoder");							/* displays the string on lcd*/
#endif
}

void write_lcd_char(unsigned char value)
{
#if (LCD_ACTIVE)
  test = value;
  lcd_clrscr();							/* clear the screen*/
  sprintf(string, "Hex: %X", value);
  lcd_puts(string);						/* displays the string on lcd*/
#endif
}

void write_lcd_char2(unsigned char value)
{
#if (LCD_ACTIVE)
  test = value;
  lcd_gotoxy(0,1);
  sprintf(string, "Hex: %X", value);
  lcd_puts(string);						/* displays the string on lcd*/
#endif
}

void write_lcd_int(unsigned int value)
{
#if (LCD_ACTIVE)
  lcd_clrscr();							/* clear the screen*/
  sprintf(string, "Int: %d", value);
  lcd_puts(string);						/* displays the string on lcd*/
#endif
}

void write_lcd_int2(unsigned int value)
{
#if (LCD_ACTIVE)
  lcd_gotoxy(0,1);
  sprintf(string, "Int: %d", value);
  lcd_puts(string);						/* displays the string on lcd*/
#endif
}

void write_lcd_string(const char *s)
{
#if (LCD_ACTIVE)
  lcd_clrscr();							/* clear the screen*/
  lcd_puts(s);							/* displays the string on lcd*/
#endif
}

void write_lcd_string2(const char *s)
{
#if (LCD_ACTIVE)
  lcd_gotoxy(0,1);
  lcd_puts(s);							/* displays the string on lcd*/
#endif
}

//-----------------------------------------------------------------------------------
