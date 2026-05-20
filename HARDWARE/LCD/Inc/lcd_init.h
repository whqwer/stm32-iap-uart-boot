#ifndef _LCD_INIT_H_
#define _LCD_INIT_H_

#include "spi.h"
#include <stdint.h>

/* LCD display configuration */
#define USE_HORIZONTAL 1  /* Display orientation: 0=normal, 1=rotate 180 degrees */

#define LCD_W 240  /* LCD width in pixels  */
#define LCD_H 120  /* LCD height in pixels */

/* Function declarations */
void LCD_Address_Set(uint16_t xs, uint16_t ys, uint16_t xe, uint16_t ye);
void LCD_Fill(uint16_t xs, uint16_t ys, uint16_t xe, uint16_t ye, uint16_t color);
void LCD_Init(void);

/* RGB565 color definitions used by bootloader */
#define WHITE   0xFFFF
#define BLACK   0x0000
#define RED     0xF800
#define BLUE    0x001F
#define GREEN   0x07E0
#define YELLOW  0xFFE0

#endif
