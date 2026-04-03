#ifndef _LCD_H_
#define _LCD_H_

#include "lcd_init.h"

/* lcd_boot.c: font-32 string display (one char at a time, small buffer) */
void LCD_ShowStringDMA(uint16_t x, uint16_t y, const char *s,
                       uint16_t fc, uint16_t bc, uint16_t sizey);

/* Upgrading dots animation */
void app_upgrade_start(void);           /* clear + draw "Upgrading" + reset phase */
void app_upgrade_progress_tick(void);   /* advance dots: blank→.→..→...→blank */

#endif
