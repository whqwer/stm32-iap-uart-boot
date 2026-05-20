#ifndef _LCDFONT_H_
#define _LCDFONT_H_

#include <stdint.h>

/* Full font table – used by APP build (lcd.c) */
extern const unsigned char ascii_3216[][64];

/* Stripped font table – used by bootloader build (lcd_boot.c) */
typedef struct {
    uint8_t  ch;        /* ASCII character value */
    uint8_t  bitmap[64]; /* 32×16 px glyph, LSB-first */
} Font3216Entry_t;

extern const Font3216Entry_t ascii_boot[];
extern const uint8_t         ascii_boot_count;

#endif



