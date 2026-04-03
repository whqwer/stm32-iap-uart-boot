#ifndef _LCD_INIT_H_
#define _LCD_INIT_H_

#include "spi.h"

/* LCD display configuration */
#define USE_HORIZONTAL 1  // Display orientation: 0=normal, 1=rotate 180 degrees

#define LCD_W 240  // LCD width in pixels
#define LCD_H 120  // LCD height in pixels

/* Function declarations */
void LCD_Address_Set(uint16_t xs, uint16_t ys, uint16_t xe, uint16_t ye);          // Set display window
void LCD_Fill(uint16_t xs, uint16_t ys, uint16_t xe, uint16_t ye, uint16_t color); // Fill area with color
void LCD_Init(void);                                                               // Initialize LCD
void LCD_Sleep(void);                                                              // Enter sleep mode (display off)

/* Common RGB565 color definitions */
#define WHITE 0xFFFF
#define BLACK 0x0000
#define BLUE 0x001F
#define BRED 0XF81F
#define GRED 0XFFE0
#define GBLUE 0X07FF
#define MAGENTA 0xF81F
#define GREEN 0x07E0
#define CYAN 0x7FFF
#define YELLOW 0xFFE0
#define BROWN 0XBC40      // Brown
#define BRRED 0XFC07      // Brownish red
#define GRAY 0X8430       // Gray
#define DARKBLUE 0X01CF   // Dark blue
#define LIGHTBLUE 0X7D7C  // Light blue
#define GRAYBLUE 0X5458   // Gray blue
#define LIGHTGREEN 0X841F // Light green
#define LGRAY 0XC618      // Light gray (PANEL), window background color
#define LGRAYBLUE 0XA651  // Light gray blue (dialog header)
#define LBBLUE 0X2B12     // Light blue (selected item highlight color)

/* Red color brightness levels (RGB565 format, R5G6B5) */
extern volatile uint8_t red_level; // 当前亮度级别，0~10
extern volatile uint16_t RED;      // 当前实际颜色值

#define RED_LEVEL_MAX 9
#define RED_LEVEL_MIN 0

/* Red brightness level enumeration */
static const uint16_t red_table[10] = {0xF800,0xE000,0xC800,0xB000,0x9800,0x8000,0x6800,0x5000,0x3800,0x2000};

void LCD_UpdateRed(void);

#endif
