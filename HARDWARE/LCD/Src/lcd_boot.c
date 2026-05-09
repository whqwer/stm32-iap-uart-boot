/*
 * lcd_boot.c – Minimal LCD text rendering for bootloader
 *
 * Purpose:
 *   lcd.c is excluded from the bootloader build to avoid its large RAM buffers
 *   (screen[4096], buff1206_str[4608]).  This file provides a lightweight
 *   LCD_ShowStringDMA that supports font-32 only, using a small 1-KB static
 *   buffer to render one character at a time via DMA.
 *
 *   Also provides the "Upgrading…" dots animation used during OTA:
 *     app_upgrade_start()         — clear screen, draw "Upgrading", reset phase
 *     app_upgrade_progress_tick() — cycle "." → ".." → "..." → (blank) → …
 *
 * Memory overhead: 1024 bytes (lcd_char_buf) in BSS, 32-byte aligned.
 *
 * Font layout (ascii_3216):
 *   Each character entry is 64 bytes (32 rows × 2 bytes = 4 bytes/row × 8 rows).
 *   For each byte, bits are ordered LSB-first: bit-0 = leftmost pixel in the row.
 *   LCD window per character: 32 px wide × 16 px tall (sizey=32, sizex=16).
 *   Characters advance in the Y direction: y += 16 per character.
 */

#include "lcd_init.h"   /* LCD_Fill, LCD_Address_Set, color macros */
#include "spi_func.h"   /* LCD_WR_Busbuf */
#include "lcdfont.h"    /* ascii_3216[][64] */

/* ── One-character pixel buffer ──────────────────────────────────────────── */
/* 32 × 16 pixels × 2 bytes/pixel = 1024 bytes; must be 32-byte aligned for
 * STM32H5 D-cache coherency when used as a DMA source.                       */
__ALIGNED(32) static uint8_t lcd_char_buf[1024];

/* ── LCD_ShowStringDMA ───────────────────────────────────────────────────── */
/**
 * @brief  Display an ASCII string using font-32, advancing vertically (Y axis).
 *
 * Each character occupies a 32 × 16 px window (width=32, height=16).
 * Characters are stacked downward: y increments by 16 per character, x is fixed.
 * This matches the coordinate layout used in the original lcd.c and the lens app.
 *
 * @param x      Column (horizontal) start.  Centered: x = (120-32)/2 = 44.
 * @param y      Row    (vertical)   start.  E.g. 40, 88, 136 for "Upgrading".
 * @param s      NUL-terminated ASCII string (printable chars only).
 * @param fc     Foreground colour (RGB565).
 * @param bc     Background colour (RGB565).
 * @param sizey  Character height in pixels; only 32 is supported.
 */
void LCD_ShowStringDMA(uint16_t x, uint16_t y, const char *s,
                       uint16_t fc, uint16_t bc, uint16_t sizey)
{
    if (sizey != 32u) return;           /* only font-32 supported here */

    const uint8_t  sizex       = 16u;  /* char width  = sizey / 2     */
    const uint16_t TypefaceNum = 64u;  /* bytes per glyph in ascii_3216 */

    for (; *s >= ' ' && *s <= '~'; ++s) {
        uint8_t num = (uint8_t)(*s) - (uint8_t)' ';
        if (num >= 96u) break;          /* out of table range, stop    */

        /* Set LCD window: 32 px wide (columns x…x+31), 16 px tall (rows y…y+15) */
        LCD_Address_Set(x, y, x + (uint16_t)(sizey - 1u), y + (uint16_t)(sizex - 1u));

        /* Render glyph into lcd_char_buf.
         * Layout: 64 font bytes, 4 bytes per row, 16 rows total.
         * Within each byte, bit-0 is the leftmost pixel (LSB-first).
         * After every 32 pixels (= 4 bytes × 8 bits), a new row starts. */
        uint32_t cnt = 0u, m = 0u;
        for (uint16_t i = 0u; i < TypefaceNum; ++i) {
            uint8_t temp = ascii_3216[num][i];
            for (uint8_t t = 0u; t < 8u; ++t) {
                uint16_t color = ((temp >> t) & 0x01u) ? fc : bc;
                lcd_char_buf[cnt++] = (uint8_t)(color >> 8u);
                lcd_char_buf[cnt++] = (uint8_t)(color & 0xFFu);
                if (++m % (uint32_t)sizey == 0u) { m = 0u; break; }
            }
        }
        /* cnt == 1024 (512 pixels × 2 bytes).  LCD_WR_Busbuf sets DC=1
         * and waits for DMA completion internally.                       */
        LCD_WR_Busbuf(lcd_char_buf, cnt);

        y += sizex;                     /* advance to next character row */
    }
}

/* ── Upgrading animation ─────────────────────────────────────────────────── */
/* Layout (portrait 120×240):
 *   "Upgrading" = 9 chars × 16 px = 144 px → y: 40 → 184
 *   Dots area   =  3 chars × 16 px =  48 px → y: 184 → 232
 *   Character width = 32 px, centred at x = (120-32)/2 = 44.             */
#define UPGRADE_X     44u
#define UPGRADE_DOT_Y 184u

static uint8_t s_dot_phase = 0u;   /* 0 = blank, 1 = ".", 2 = "..", 3 = "..." */
static const char * const k_dot_str[4] = { "", ".", "..", "..." };

/**
 * @brief  Clear screen and draw "Upgrading" text; reset dot animation phase.
 *         Call once before entering IAP_Update().
 */
void app_upgrade_start(void)
{
    LCD_Fill(0, 0, 120, 240, BLACK);

    LCD_ShowStringDMA(UPGRADE_X,  32u, "Upgrading", WHITE, BLACK, 32);

    s_dot_phase = 0u;   /* tick() will advance to 1 (".")  on first call */
}

/**
 * @brief  Animate the dots below "Upgrading".  Call every ~500 ms from
 *         the IAP_Update() main loop (after DMA has been restarted).
 *
 * Cycle: blank → "." → ".." → "..." → blank → …
 * Each call erases the dots area and redraws the current phase.
 */
void app_upgrade_progress_tick(void)
{
    s_dot_phase = (uint8_t)((s_dot_phase + 1u) % 4u);

    /* Erase dots area: x=44..75 (32 px wide), y=184..239 */
    LCD_Fill(UPGRADE_X, UPGRADE_DOT_Y,
             (uint16_t)(UPGRADE_X + 32u), 240u, BLACK);

    if (s_dot_phase != 0u) {
        LCD_ShowStringDMA(UPGRADE_X, UPGRADE_DOT_Y,
                          (char *)k_dot_str[s_dot_phase], WHITE, BLACK, 32);
    }
}
